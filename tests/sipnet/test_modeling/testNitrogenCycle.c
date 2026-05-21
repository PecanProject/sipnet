#include "utils/tUtils.h"
#include "sipnet/events.c"
#include "sipnet/nitrogen.c"
#include "sipnet/limitations.c"

/////
// Setup and general test state management
void initGeneralState(void) {
  // Static for all tests, just needs to be set once
  envi.soilWater = 5.0;
  envi.soilC = 1.5;
  envi.litterC = 1;

  params.minNInit = 0.0;
  params.soilWHC = 10.0;

  // Values from russell_2 smoke test
  params.soilRespMoistEffect = 1.0;
  params.baseSoilResp = 0.06;
  params.soilRespQ10 = 2.9;
  params.leafCN = 20.0;
  params.woodCN = 100.0;
  params.fineRootCN = 40.0;
}

void resetState() {
  // State altered by one test or another, and needs to be reset at the
  // start of each test

  // Respiration/Mineralization
  fluxes.rSoil = 0.0;
  fluxes.rLitter = 0.0;
  fluxes.nMin = 0.0;

  // Volatilization
  params.nVolatilizationFrac = 0;
  fluxes.nVolatilization = 0;
  // Leaching
  params.nLeachingFrac = 0;
  fluxes.nLeaching = 0;
  // Fixation/Uptake
  params.nFixationFracMax = 0;
  params.halfNFixationMax = 0;
  fluxes.nFixation = 0;
  fluxes.nUptake = 0;
  // Plant storage N and leaf-off resorption flux (new in SIP336)
  envi.plantStorageN = 0.0;
  fluxes.leafOffNResorption = 0.0;
}

void setupTests(void) {
  // set up dummy climate
  climate = (ClimateNode *)malloc(sizeof(ClimateNode));
  climate->year = 2024;
  climate->day = 70;
  climate->length = 0.125;
  climate->tsoil = 20.0;

  // Set up the context
  initContext();
  ctx.litterPool = 1;
  ctx.nitrogenCycle = 1;
  ctx.anaerobic = 1;

  // Initialize general state
  initGeneralState();
}

int checkFlux(double calcFlux, double expFlux, const char *label) {
  // Make sure we didn't forget to update context, in case dependencies changed
  validateContext();

  int status = 0;
  if (!compareDoubles(calcFlux, expFlux)) {
    status = 1;
    logTest("Calculated %s flux is %f, expected %f\n", label, calcFlux,
            expFlux);
  }

  return status;
}

/////
// N Volatilization
void initNVolatilizationState(double initN, double nVol) {
  resetState();
  envi.minN = initN;
  params.nVolatilizationFrac = nVol;
}

int testNVolatilization(void) {
  int status = 0;
  double minN;
  double nVolFrac;
  double expNVolFlux;
  logTest("Running testNVolatilization\n");

  minN = 2;
  nVolFrac = 0.1;
  initNVolatilizationState(minN, nVolFrac);
  double tEffect = calcTempEffect(climate->tsoil);
  double mEffect =
      calcVolatilizationMoistEffect(envi.soilWater, params.soilWHC);
  expNVolFlux = nVolFrac * minN * tEffect * mEffect;
  calcNVolatilizationFlux();
  status |= checkFlux(fluxes.nVolatilization, expNVolFlux, "N volatilization");

  // easy proportionality test - doubling params should double output
  minN *= 2;
  expNVolFlux *= 2;
  initNVolatilizationState(minN, nVolFrac);
  calcNVolatilizationFlux();
  status |= checkFlux(fluxes.nVolatilization, expNVolFlux, "N volatilization");

  // Check minN for the last
  updateNitrogenPools();
  double expMinN = minN - expNVolFlux * climate->length;
  int minStatus = 0;
  if (!compareDoubles(envi.minN, expMinN)) {
    minStatus = 1;
    logTest("minN pool is %8.3f, expected %8.3f\n", envi.minN, expMinN);
  }
  status |= minStatus;

  return status;
}

/////
// N Fertilization
// (uses volatilization state management)
int testFertilization(void) {
  int status = 0;
  double initN = 2.0;
  double nVolFrac = 0.1;
  double expMinN, expEventMinNFlux, expNVolFlux;
  logTest("Running testFertilization\n");

  // init minN 2, nVol 0.1
  initNVolatilizationState(initN, nVolFrac);

  // fert event: 15 5 10
  double fertMinN = 10;
  initEvents("events_fert.in", "events.out", 0);
  setupEvents();

  calcNVolatilizationFlux();
  processEvents();
  updateNitrogenPools();
  updatePoolsForEvents();

  // Want to test:
  // envi: minN
  // fluxes: nVol, eventMinN
  double tEffect = calcTempEffect(climate->tsoil);
  double mEffect =
      calcVolatilizationMoistEffect(envi.soilWater, params.soilWHC);
  expNVolFlux = (nVolFrac * initN * tEffect * mEffect);
  expEventMinNFlux = fertMinN / climate->length;
  expMinN = initN + (expEventMinNFlux - expNVolFlux) * climate->length;

  if (!compareDoubles(fluxes.nVolatilization, expNVolFlux)) {
    status = 1;
    logTest("fluxes.nVolatilization is %8.4f, expected %8.4f\n",
            fluxes.nVolatilization, expNVolFlux);
  }
  if (!compareDoubles(fluxes.eventMinN, expEventMinNFlux)) {
    status = 1;
    logTest("fluxes.eventMinN is %8.4f, expected %8.4f\n", fluxes.eventMinN,
            expEventMinNFlux);
  }
  if (!compareDoubles(envi.minN, expMinN)) {
    status = 1;
    logTest("envi.minN is %8.4f, expected %8.4f\n", envi.minN, expMinN);
  }

  return status;
}

/////
// N Leaching
void initNLeachingState(double initN, double nLeachFrac) {
  resetState();
  envi.minN = initN;
  params.nLeachingFrac = nLeachFrac;
}

int testNLeaching(void) {
  int status = 0;
  double minN;
  double nLeachFrac;
  double expNLeaching;
  double phi;
  logTest("Running testNLeaching\n");

  minN = 1;
  nLeachFrac = 0.5;
  fluxes.drainage = 5;
  initNLeachingState(minN, nLeachFrac);
  if ((fluxes.drainage / params.soilWHC) < 1) {
    phi = fluxes.drainage / params.soilWHC;
  } else {
    phi = 1;
  }
  expNLeaching = minN * phi * nLeachFrac;
  calcNLeachingFlux();
  status |= checkFlux(fluxes.nLeaching, expNLeaching, "N leaching");

  minN = 1;
  nLeachFrac = 0.5;
  fluxes.drainage = 20;
  initNLeachingState(minN, nLeachFrac);
  if ((fluxes.drainage / params.soilWHC) < 1) {
    phi = fluxes.drainage / params.soilWHC;
  } else {
    phi = 1;
  }
  expNLeaching = minN * phi * nLeachFrac;
  calcNLeachingFlux();
  status |= checkFlux(fluxes.nLeaching, expNLeaching, "N leaching");

  // Check minN for the last
  updateNitrogenPools();
  double expMinN = minN - (expNLeaching * climate->length);
  int minStatus = 0;
  if (!compareDoubles(envi.minN, expMinN)) {
    minStatus = 1;
    logTest("minN pool is %8.3f, expected %8.3f\n", envi.minN, expMinN);
  }
  status |= minStatus;

  return status;
}

/////
// N Fixation and Uptake
void initNFixationState(double initN, double nFixFracMax, double nFixHalved) {
  resetState();
  envi.minN = initN;
  params.nFixationFracMax = nFixFracMax;
  params.halfNFixationMax = nFixHalved;

  // We need some demand. For convenience:
  //  leafCN = 20
  //  woodCN = 100
  //  fineRootCN = 40
  fluxes.leafOnCreation = 0;  // too messy
  fluxes.leafCreation = 60;  // 3 demand flux
  fluxes.woodCreation = 500;  // 5
  fluxes.fineRootCreation = 40;  // 1
  fluxes.coarseRootCreation = 100;  // 1 = 10 total
}

int checkFinalMinN(double initMinN, double expMinN) {
  int status = 0;
  updateNitrogenPools();
  if (!compareDoubles(envi.minN, expMinN)) {
    status = 1;
    logTest("For init minN %6.3f: minN pool is %8.3f, expected %8.3f\n",
            initMinN, envi.minN, expMinN);
  }

  return status;
}

void doNFixUpLimitCalcs(void) {
  calcNFixationAndUptakeFluxes();
  checkNitrogenLimitation();
}

int testNFixation(void) {
  int status = 0;
  double minN;
  double nFixFracMax;
  double nFixHalved;
  double expNFixation;
  double expNUptake;
  double nDemand;
  double expMinN;
  logTest("Running testNFixation\n");

  // Plenty of min N
  minN = 4;
  nFixFracMax = 1;
  nFixHalved = 2;
  nDemand = 10;
  initNFixationState(minN, nFixFracMax, nFixHalved);
  double nFixInhib = nFixHalved / (nFixHalved + minN);
  double nFixFrac = nFixFracMax * nFixInhib;
  expNFixation = nFixFrac * nDemand;
  expNUptake = (1 - nFixFrac) * nDemand;
  doNFixUpLimitCalcs();
  status |= checkFlux(fluxes.nFixation, expNFixation, "N fixation");
  status |= checkFlux(fluxes.nUptake, expNUptake, "N uptake");
  expMinN = minN - expNUptake * climate->length;
  status |= checkFinalMinN(minN, expMinN);

  // Just above min N needed
  minN = 1;
  nFixFracMax = 0.75;
  nFixHalved = 1;
  nDemand = 10;
  initNFixationState(minN, nFixFracMax, nFixHalved);
  nFixInhib = nFixHalved / (nFixHalved + minN);
  nFixFrac = nFixFracMax * nFixInhib;
  expNFixation = nFixFrac * nDemand;
  expNUptake = (1 - nFixFrac) * nDemand;
  doNFixUpLimitCalcs();
  status |= checkFlux(fluxes.nFixation, expNFixation, "N fixation");
  status |= checkFlux(fluxes.nUptake, expNUptake, "N uptake");
  expMinN = minN - expNUptake * climate->length;
  status |= checkFinalMinN(minN, expMinN);

  // Just below min N needed; limitation will hit, should be 20% reduction
  double red = 0.8;
  minN = 0.5;
  nFixFracMax = 0.75;
  nFixHalved = 1;
  nDemand = 10;
  initNFixationState(minN, nFixFracMax, nFixHalved);
  nFixInhib = nFixHalved / (nFixHalved + minN);
  nFixFrac = nFixFracMax * nFixInhib;
  expNFixation = nFixFrac * nDemand * red;
  expNUptake = (1 - nFixFrac) * nDemand * red;
  doNFixUpLimitCalcs();
  status |= checkFlux(fluxes.nFixation, expNFixation, "N fixation");
  status |= checkFlux(fluxes.nUptake, expNUptake, "N uptake");
  expMinN = minN - expNUptake * climate->length;
  status |= checkFinalMinN(minN, expMinN);

  // Zero minN (which really shouldn't happen, but still good to test edge case)
  // leads to minimal growth due to N limitation - but mineralization lets some
  // happen
  minN = 0;
  nFixFracMax = 0.5;
  nFixHalved = 2;
  initNFixationState(minN, nFixFracMax, nFixHalved);
  doNFixUpLimitCalcs();
  status |= checkFlux(fluxes.nFixation, 0, "N fixation");
  status |= checkFlux(fluxes.nUptake, 0, "N uptake");
  expMinN = 0;
  status |= checkFinalMinN(minN, expMinN);

  return status;
}

/////
// N Limitation effects on plant growth fluxes
void initNLimitationState(double initN, double initLeafOnCreation) {
  resetState();
  envi.minN = initN;
  params.nFixationFracMax = 0;  // No N fixation for clean testing
  params.halfNFixationMax = 0;

  // Demand setup (same layout as initNFixationState):
  //  leafCN = 20, woodCN = 100, fineRootCN = 40
  fluxes.leafOnCreation = initLeafOnCreation;
  fluxes.leafCreation = 60;  // 3 demand flux
  fluxes.woodCreation = 500;  // 5
  fluxes.fineRootCreation = 40;  // 1
  fluxes.coarseRootCreation = 100;  // 1; 10 total demand without leafOnCreation
}

int checkNLimitationFlux(double flux, double exp, const char *label) {
  int status = 0;
  if (!compareDoubles(flux, exp)) {
    status = 1;
    logTest("%s is %8.4f, expected %8.4f\n", label, flux, exp);
  }
  return status;
}

int testNLimitation(void) {
  int status = 0;
  logTest("Running testNLimitation\n");

  // N limitation with no fixation - 50% reduction
  // maxDemandFlux = 10, maxDemand = 10 * 0.125 = 1.25, maxUptake = 1.25
  // availableMinN = 0.625 -> reduction = 0.625 / 1.25 = 0.5
  double reduction = 0.5;
  initNLimitationState(0.625, 0);

  doNFixUpLimitCalcs();

  status |=
      checkNLimitationFlux(fluxes.leafCreation, 60 * reduction, "leafCreation");
  status |= checkNLimitationFlux(fluxes.woodCreation, 500 * reduction,
                                 "[50%] woodCreation");
  status |= checkNLimitationFlux(fluxes.fineRootCreation, 40 * reduction,
                                 "[50%] fineRootCreation");
  status |= checkNLimitationFlux(fluxes.coarseRootCreation, 100 * reduction,
                                 "[50%] coarseRootCreation");

  // Sufficient mineralization (nMin) prevents N limitation
  // minN=0.1, nMin=12 -> availableMinN = 0.1 + 12 * 0.125 = 1.6 > 1.25
  // -> no limitation, all creation fluxes stay at original values
  initNLimitationState(0.1, 0);
  fluxes.nMin = 12.0;

  doNFixUpLimitCalcs();
  status |= checkNLimitationFlux(fluxes.leafCreation, 60,
                                 "[boosted nMin] leafCreation");
  status |= checkNLimitationFlux(fluxes.woodCreation, 500,
                                 "[boosted nMin] woodCreation");

  // leafOnCreation also gets reduced when N-limited.
  // leaf-on N is NOT part of calcPlantNDemand(); instead it reduces
  // calcPlantAvailableN() via unclaimedStorage:
  //   leafOnNFlux = max(0, 50/20 - 50/100) = 2.0
  //   unclaimedStorage = plantStorageN + (0 - 2.0) * 0.125 = -0.25
  //   availableN = max(0, minN=0.75 + (-0.25)) = 0.5
  //   demand (excl. leafOn) = 10, maxDemand = 10 * 0.125 = 1.25, maxUptake = 1.25
  //   reduction = 0.5 / 1.25 = 0.4
  double leafOnInit = 50.0;
  double leafOnReduction = 0.4;
  initNLimitationState(0.75, leafOnInit);

  doNFixUpLimitCalcs();

  status |=
      checkNLimitationFlux(fluxes.leafOnCreation, leafOnInit * leafOnReduction,
                           "[leafOn] leafOnCreation");

  return status;
}

/////
// Organic N pools
void initOrganicNState(double initLitterN, double initSoilN) {
  resetState();

  // envi
  envi.minN = 1;
  envi.litterC = 2;
  envi.soilC = 3;
  envi.litterN = initLitterN;
  envi.soilOrgN = initSoilN;

  // fluxes; these values make all terms in flux calc equal to 1 for
  // easy comparison
  fluxes.leafLitter = params.leafCN;
  fluxes.woodLitter = params.woodCN;
  fluxes.fineRootLoss = params.fineRootCN;
  fluxes.coarseRootLoss = params.woodCN;
  fluxes.rLitter = envi.litterC / envi.litterN;
  fluxes.litterToSoil = envi.soilC / envi.soilOrgN;
  fluxes.rSoil = envi.soilC / envi.soilOrgN;
}

int testOrganicN(void) {
  int status = 0;
  double minN, litterN, soilOrgN;
  double expSoilOrgN, expLitterN;
  logTest("Running testOrganicN\n");

  // test
  minN = 1;
  litterN = 2;
  soilOrgN = 3;
  initOrganicNState(litterN, soilOrgN);
  expSoilOrgN = 2;
  expLitterN = 0;
  calcNPoolFluxes();

  status |= checkFlux(fluxes.nOrgLitter, expLitterN, "Organic litter N");
  status |= checkFlux(fluxes.nOrgSoil, expSoilOrgN, "Organic soil N");

  // Check minN for the last - it should have increased from mineralization
  updateNitrogenPools();
  double expMinN = minN + 2 * climate->length;
  int minStatus = 0;
  if (!compareDoubles(envi.minN, expMinN)) {
    minStatus = 1;
    logTest("minN pool is %8.3f, expected %8.3f\n", envi.minN, expMinN);
    logTest("nMin flux is %8.3f\n", fluxes.nMin);
  }
  status |= minStatus;

  return status;
}

/////
// Organic N with leafOffNResorption
int testOrganicNWithResorption(void) {
  int status = 0;
  logTest("Running testOrganicNWithResorption\n");

  // Same setup as testOrganicN but with non-zero leafOffNResorption.
  // Without resorption nOrgLitter = 0 (from testOrganicN).
  // With leafOffNResorption=0.5 subtracted:
  //   nOrgLitter = leafLitter/leafCN - 0.5 + woodLitter/woodCN - litterMin -
  //   litterToSoil/litterCN
  //              = 1 - 0.5 + 1 - 1 - 1 = -0.5
  initOrganicNState(2.0, 3.0);
  fluxes.leafOffNResorption = 0.5;

  calcNPoolFluxes();

  status |= checkFlux(fluxes.nOrgLitter, -0.5,
                      "Organic litter N with leafOffNResorption");

  return status;
}

/////
// N limitation with plantStorageN contributing to available N
int testNLimitationWithStorage(void) {
  int status = 0;
  logTest("Running testNLimitationWithStorage\n");

  // Reproduce the 50%-limited case from testNLimitation, then show that
  // adding plantStorageN = minN removes the limitation entirely.
  // maxDemandFlux = 10, maxDemand = 10 * 0.125 = 1.25, maxUptake = 1.25
  // With minN=0.625 alone: availableMinN=0.625 -> reduction=0.5
  // With minN=0.625 + plantStorageN=0.625: availableMinN=1.25 >= maxUptake ->
  // no limit
  initNLimitationState(0.625, 0);
  envi.plantStorageN = 0.625;

  doNFixUpLimitCalcs();

  // All creation fluxes should be unreduced
  status |= checkNLimitationFlux(fluxes.leafCreation, 60.0,
                                 "[storageN] leafCreation");
  status |= checkNLimitationFlux(fluxes.woodCreation, 500.0,
                                 "[storageN] woodCreation");
  status |= checkNLimitationFlux(fluxes.fineRootCreation, 40.0,
                                 "[storageN] fineRootCreation");
  status |= checkNLimitationFlux(fluxes.coarseRootCreation, 100.0,
                                 "[storageN] coarseRootCreation");

  return status;
}

/////
// updateNitrogenPools draws uptake from plantStorageN before minN
int testUpdateNitrogenPoolsFromStorage(void) {
  int status = 0;
  logTest("Running testUpdateNitrogenPoolsFromStorage\n");

  // Case 1: uptake fully covered by storage
  // nUptake=3.0, length=0.125 -> uptake=0.375; plantStorageN=0.5 >= 0.375
  // -> all uptake from storage, minN unchanged
  resetState();
  envi.minN = 1.0;
  envi.plantStorageN = 0.5;
  fluxes.nUptake = 3.0;
  updateNitrogenPools();

  double expStorageN = 0.5 - 3.0 * climate->length;  // 0.5 - 0.375 = 0.125
  if (!compareDoubles(envi.plantStorageN, expStorageN)) {
    status = 1;
    logTest("[full storage] plantStorageN is %8.4f, expected %8.4f\n",
            envi.plantStorageN, expStorageN);
  }
  if (!compareDoubles(envi.minN, 1.0)) {
    status = 1;
    logTest("[full storage] minN is %8.4f, expected 1.0 (unchanged)\n",
            envi.minN);
  }

  // Case 2: uptake partially covered by storage, remainder from minN
  // plantStorageN=0.1 < uptake=0.375 -> 0.1 from storage, 0.275 from minN
  // plantStorageN_new = 0.0, minN_new = 1.0 - 0.275 = 0.725
  resetState();
  envi.minN = 1.0;
  envi.plantStorageN = 0.1;
  fluxes.nUptake = 3.0;
  updateNitrogenPools();

  if (!compareDoubles(envi.plantStorageN, 0.0)) {
    status = 1;
    logTest("[partial storage] plantStorageN is %8.4f, expected 0.0\n",
            envi.plantStorageN);
  }
  double expMinN = 1.0 - (3.0 * climate->length - 0.1);  // 1.0 - 0.275 = 0.725
  if (!compareDoubles(envi.minN, expMinN)) {
    status = 1;
    logTest("[partial storage] minN is %8.4f, expected %8.4f\n", envi.minN,
            expMinN);
  }

  return status;
}

int run(void) {
  int status = 0;

  setupTests();

  status |= testNVolatilization();
  status |= testFertilization();
  status |= testNLeaching();
  status |= testOrganicN();
  status |= testNFixation();
  status |= testNLimitation();
  status |= testNLimitationWithStorage();
  status |= testUpdateNitrogenPoolsFromStorage();
  status |= testOrganicNWithResorption();

  return status;
}

int main(void) {
  int status;

  logTest("Starting testNitrogenCycle:run()\n");
  status = run();
  if (status) {
    logTest("FAILED testNitrogenCycle with status %d\n", status);
    exit(status);
  }

  logTest("PASSED testNitrogenCycle\n");
}

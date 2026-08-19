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

  // Reset all the fluxes
  fluxes = (struct FluxVars){0};

  // Params
  // Volatilization
  params.nVolatilizationFrac = 0;
  // Leaching
  params.nLeachingFrac = 0;
  // Fixation/Uptake
  params.nFixationFracMax = 0;
  params.halfNFixationMax = 0;

  // Pools
  // Plant storage N and leaf-off resorption flux (new in SIP336)
  envi.plantStorageN = 0.0;
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

  // leafOnCreation is NOT reduced by soil N limitation; leaf-on draws N from
  // plantStorageN, not minN. However, leaf-on demand reduces available soil N
  // for other fluxes via unclaimedStorage. Note that leaf-on won't draw past
  // plantStorageN
  //   leafOnNFlux = max(0, 50/20 - 50/100) = 2.0
  //   unclaimedStorage = plantStorageN + (0 - 2.0) * 0.125 = 0.25
  //   availableN = minN=0.75 + 0=nonUptakeFluxes) = 0.75
  //   demand (excl. leafOn) = 10, maxDemand = 10 * 0.125 = 1.25,
  //   reduction = (availableN / (1-f) + S)/maxDemand, f = 0
  //             = (0.75 + 0.25 ) / 1.25 = 0.8
  double leafOnInit = 50.0;
  double leafOnReduction = 0.8;
  initNLimitationState(0.75, leafOnInit);
  envi.plantStorageN = 0.5;

  doNFixUpLimitCalcs();

  // leafOnCreation is unchanged (not reduced by soil N limitation)
  status |= checkNLimitationFlux(fluxes.leafOnCreation, leafOnInit,
                                 "[leafOn] leafOnCreation");
  // other growth fluxes are still reduced by 0.4 because leaf-on's N demand
  // reduces available soil N through unclaimedStorage
  status |= checkNLimitationFlux(fluxes.woodCreation, 500 * leafOnReduction,
                                 "[leafOn] woodCreation");
  status |= checkNLimitationFlux(fluxes.leafCreation, 60 * leafOnReduction,
                                 "[leafOn] leafCreation");

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

int checkMinAndStorageN(const char *prefix, double expMinN,
                        double expStorageN) {
  int status = 0;
  if (!compareDoubles(envi.minN, expMinN)) {
    status = 1;
    logTest("[%s] minN is %8.4f, expected %8.4f\n", prefix, envi.minN,
            expStorageN);
  }
  if (!compareDoubles(envi.plantStorageN, expStorageN)) {
    status = 1;
    logTest("[%s] plantStorageN is %8.4f, expected %8.4f\n", prefix,
            envi.plantStorageN, expStorageN);
  }
  return status;
}

/////
// N limitation with plantStorageN contributing to available N
int testNLimitationWithStorage(void) {
  int status = 0;
  logTest("Running testNLimitationWithStorage\n");

  // Case 1
  // Reproduce the 50%-limited case from testNLimitation, then show that
  // adding plantStorageN = minN removes the limitation entirely.
  // maxDemandFlux = 10, maxDemand = 10 * 0.125 = 1.25, maxUptake = 1.25
  // With minN=0.625 alone: availableMinN=0.625 -> reduction=0.5
  // With minN=0.625 + plantStorageN=0.625: availableMinN=1.25 >= maxUptake ->
  // no limit
  initNLimitationState(0.625, 0);
  envi.plantStorageN = 0.625;

  doNFixUpLimitCalcs();
  updateNitrogenPools();

  // All creation fluxes should be unreduced
  status |= checkNLimitationFlux(fluxes.leafCreation, 60.0,
                                 "[storageN] leafCreation");
  status |= checkNLimitationFlux(fluxes.woodCreation, 500.0,
                                 "[storageN] woodCreation");
  status |= checkNLimitationFlux(fluxes.fineRootCreation, 40.0,
                                 "[storageN] fineRootCreation");
  status |= checkNLimitationFlux(fluxes.coarseRootCreation, 100.0,
                                 "[storageN] coarseRootCreation");
  status |= checkMinAndStorageN("storageN", 0.0, 0.0);

  // Case 2
  // Check this situation with fixation:
  // minN=0.1, plantStorageN=0.5, nFixationFracMax=0.5, halfNFixationMax=1.0,
  // timestep 0.125, standard demand fluxes 60/500/40/100) and after
  // calcNFixationAndUptakeFluxes() + checkLimitations() + updateNitrogenPools()
  // minN ended at -0.227273 (from copilot)
  initNLimitationState(0.1, 0);
  envi.plantStorageN = 0.5;
  params.nFixationFracMax = 0.5;
  params.halfNFixationMax = 1.0;

  double len = climate->length;
  logTest("minN %f storageN %f fixation %f uptake %f demand %f  "
          "1-fixfrac %f\n",
          envi.minN, envi.plantStorageN, fluxes.nFixation * len,
          fluxes.nUptake * len, calcPlantNDemandFlux() * len,
          1 - calcNFixationFrac());

  // doNFixUpLimitCalcs();
  calcNFixationAndUptakeFluxes();

  logTest("minN %f storageN %f fixation %f uptake %f demand %f  "
          "1-fixfrac %f\n",
          envi.minN, envi.plantStorageN, fluxes.nFixation * len,
          fluxes.nUptake * len, calcPlantNDemandFlux() * len,
          1 - calcNFixationFrac());

  checkNitrogenLimitation();

  logTest("minN %f storageN %f fixation %f uptake %f demand %f  "
          "1-fixfrac %f\n",
          envi.minN, envi.plantStorageN, fluxes.nFixation * len,
          fluxes.nUptake * len, calcPlantNDemandFlux() * len,
          1 - calcNFixationFrac());

  updateNitrogenPools();

  logTest("minN %f storageN %f fixation %f uptake %f demand %f  "
          "1-fixfrac %f\n",
          envi.minN, envi.plantStorageN, fluxes.nFixation * len,
          fluxes.nUptake * len, calcPlantNDemandFlux() * len,
          1 - calcNFixationFrac());

  status |= checkMinAndStorageN("storageN", 0.0, 0.0);

  return status;
}

/////
// updateNitrogenPools draws uptake from plantStorageN before minN
void initNitrogenPoolsFromStorageState(double plantStorageN) {
  resetState();

  // envi
  envi.minN = 1;
  envi.plantStorageN = plantStorageN;

  // fluxes; these values make all terms plant N demand =2, so demand flux = 8,
  // and demand = (8*climate->length) = 1
  fluxes.leafCreation = params.leafCN * 2;
  fluxes.woodCreation = params.woodCN * 2;
  fluxes.fineRootCreation = params.fineRootCN * 2;
  fluxes.coarseRootCreation = params.woodCN * 2;
}

int testUpdateNitrogenPoolsFromStorage(void) {
  int status = 0;
  logTest("Running testUpdateNitrogenPoolsFromStorage\n");

  // Demand flux = 8 ==> demand = 1 for all cases

  // Case 1: uptake fully covered by storage
  // plantNDemand = 4, storage = 5
  // -> all uptake from storage, minN unchanged
  initNitrogenPoolsFromStorageState(2.0);
  calcNFixationAndUptakeFluxes();
  updateNitrogenPools();

  double expStorageN = 1.0;
  if (!compareDoubles(envi.plantStorageN, expStorageN)) {
    status = 1;
    logTest("[full storage] plantStorageN is %8.4f, expected %8.4f\n",
            envi.plantStorageN, expStorageN);
  }
  double expNUptake = 0.0;
  if (!compareDoubles(fluxes.nUptake, expNUptake)) {
    status = 1;
    logTest("[full storage] nUptake is %8.4f, expected %8.4f\n", fluxes.nUptake,
            expNUptake);
  }
  if (!compareDoubles(envi.minN, 1.0)) {
    status = 1;
    logTest("[full storage] minN is %8.4f, expected 1.0 (unchanged)\n",
            envi.minN);
  }

  // Case 2: uptake partially covered by storage, remainder from minN
  initNitrogenPoolsFromStorageState(0.5);
  calcNFixationAndUptakeFluxes();
  updateNitrogenPools();

  if (!compareDoubles(envi.plantStorageN, 0.0)) {
    status = 1;
    logTest("[partial storage] plantStorageN is %8.4f, expected 0.0\n",
            envi.plantStorageN);
  }
  expNUptake = 0.5 / climate->length;
  if (!compareDoubles(fluxes.nUptake, expNUptake)) {
    status = 1;
    logTest("[partial storage] nUptake is %8.4f, expected %8.4f\n",
            fluxes.nUptake, expNUptake);
  }
  double expMinN = 0.5;
  if (!compareDoubles(envi.minN, expMinN)) {
    status = 1;
    logTest("[partial storage] minN is %8.4f, expected %8.4f\n", envi.minN,
            expMinN);
  }

  return status;
}

/////
// Leaf turnover N resorption feeds plantStorageN and reduces N limitation
int testLeafTurnoverNResorption(void) {
  int status = 0;
  logTest("Running testLeafTurnoverNResorption\n");

  // Case 1: leafOffNResorption (as populated by calcLeafFluxes for leaf
  // turnover) increases plantStorageN in updateNitrogenPools.
  // plantStorageN=0, leafOffNResorption=2.0, nUptake=0 (no demand)
  //   uptake = 0 * climate->length = 0; uptakeFromStorage = min(0, 0) = 0
  //   plantStorageN_new = 0 + 2.0 * climate->length - 0 = 0.25
  resetState();
  envi.minN = 1.0;
  envi.plantStorageN = 0.0;
  fluxes.leafOffNResorption = 2.0;
  updateNitrogenPools();

  double expStorageN = 2.0 * climate->length;  // 0.25
  if (!compareDoubles(envi.plantStorageN, expStorageN)) {
    status = 1;
    logTest("[turnover resorption] plantStorageN is %8.4f, expected %8.4f\n",
            envi.plantStorageN, expStorageN);
  }

  // Case 2: UPDATE: leafOffNResorption no longer increases available N, as it
  // is not directly available to offset minN loss in the same time step. Let's
  // keep this case, and use it to verify that minN drops to zero (and not
  // negative), and plantStorageN increases as expected. Repeat case above.
  double minN2 = 0.625;
  double resorpFlux = 2.0;
  double demandFlux = 10.0;  // sum of demand fluxes set in initNLimitationState
  double availableN = minN2;  // plus unclaimed plantStorageN, which is 0 here
  double maxUptake = demandFlux * climate->length;
  double reduction = availableN / maxUptake;

  initNLimitationState(minN2, 0);
  fluxes.leafOffNResorption = resorpFlux;

  doNFixUpLimitCalcs();
  updateNitrogenPools();

  status |= checkNLimitationFlux(fluxes.leafCreation, 60 * reduction,
                                 "[turnover resorption] leafCreation");
  status |= checkNLimitationFlux(fluxes.woodCreation, 500 * reduction,
                                 "[turnover resorption] woodCreation");
  status |= checkMinAndStorageN("turnover resorption", 0.0,
                                resorpFlux * climate->length);

  return status;
}

int checkVolAndLeachingFlux(const char *prefix, double expNVol,
                            double expNLeach) {
  int status = 0;
  if (!compareDoubles(fluxes.nVolatilization, expNVol)) {
    status = 1;
    logTest("[%s] nVolatilization is %8.4f, expected %8.4f\n", prefix,
            fluxes.nVolatilization, expNVol);
  }
  if (!compareDoubles(fluxes.nLeaching, expNLeach)) {
    status = 1;
    logTest("[%s] nLeaching is %8.4f, expected %8.4f\n", prefix,
            fluxes.nLeaching, expNLeach);
  }
  return status;
}

/////
// Test our guard against vol+leaching driving min N negative
int testGuardAgainstNegativeMinN(void) {
  int status = 0;
  logTest("Running testGuardAgainstNegativeMinN\n");

  // Case 1: no leaching, as W_soil < WHC, but maxed vol
  // We'll add some mineralization too, to make sure reduction is correct in
  // that case
  // Vol
  resetState();
  fluxes.nMin = 2.0;  // 0.25 extra min N
  envi.minN = 1.0;
  params.fAnoxia = 0.6;
  params.soilWHC = 10.0;
  envi.soilWater = 8.0;  // anaerobic index = 0.5, D_water = 1
  params.soilRespQ10 = 2.5;
  climate->tsoil = 30.0;  // D_temp = 15.625
  params.nVolatilizationFrac = 1.0;  // 100% can volatilize / day
  double expNVol = 1 * 1 * 1 * 15.625;

  // We'll set up the leaching, as we need some values for this
  // Leaching
  // nLeaching = 1.0 * 0.8 * 0.5 = 0.4
  params.nLeachingFrac = 0.5;
  fluxes.drainage = 0.0;
  double expNLeach = 0.0;

  calcNVolatilizationFlux();
  calcNLeachingFlux();
  status |= checkVolAndLeachingFlux("volatilization", expNVol, expNLeach);
  // Call the limit check should reduce nVol to 10 (enough to just exhaust nMin)
  checkMineralNLimitation();
  expNVol *= 10.0 / expNVol;
  status |= checkVolAndLeachingFlux("volatilization", expNVol, 0.0);
  // Check that minN is zero at end
  updateNitrogenPools();
  status |= checkMinAndStorageN("volatilization", 0.0, 0.0);

  // Case 2: Leaching
  // We'll have a trickle of vol here, plus as much leaching as we can. Need
  // hot + wet (flooding)
  resetState();
  envi.minN = 1.0;
  // Leaching
  envi.soilWater = 20.0;
  fluxes.drainage = 10.0;  // phi = 1.0
  params.nLeachingFrac = 8.0;  // 100% leached every three hours
  expNLeach = 1.0 * 1.0 * 8.0;  // 8.0
  // Vol
  params.nVolatilizationFrac = 1.0;  // 100% can volatilize / day
  params.soilRespQ10 = 3;
  climate->tsoil = 20.0;  // D_temp = 9
  // D_water = 0.05 now
  expNVol = 1 * 1 * 0.05 * 9;  // 0.45

  calcNVolatilizationFlux();
  calcNLeachingFlux();
  status |= checkVolAndLeachingFlux("leaching", expNVol, expNLeach);
  // Call the limit check should reduce both to sum to 8
  double reduction = 8.0 / 8.45;
  expNVol *= reduction;
  expNLeach *= reduction;
  checkMineralNLimitation();
  status |= checkVolAndLeachingFlux("leaching", expNVol, expNLeach);
  // Check that minN is zero at end
  updateNitrogenPools();
  status |= checkMinAndStorageN("leaching", 0.0, 0.0);

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
  status |= testLeafTurnoverNResorption();
  status |= testGuardAgainstNegativeMinN();

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

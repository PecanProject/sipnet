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
  // for other fluxes via unclaimedStorage:
  //   leafOnNFlux = max(0, 50/20 - 50/100) = 2.0
  //   unclaimedStorage = plantStorageN + (0 - 2.0) * 0.125 = -0.25
  //   availableN = max(0, minN=0.75 + (-0.25)) = 0.5
  //   demand (excl. leafOn) = 10, maxDemand = 10 * 0.125 = 1.25,
  //   maxUptake = 1.25 -> reduction = 0.5 / 1.25 = 0.4
  double leafOnInit = 50.0;
  double leafOnReduction = 0.4;
  initNLimitationState(0.75, leafOnInit);

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

  // Case 2: leafOffNResorption from turnover increases available N in
  // calcPlantAvailableN, reducing N limitation on plant growth.
  // Reproduce the 50%-limited case (minN=0.625, demand=10) then show that
  // leafOffNResorption=2.0 raises available N and eases the limitation:
  //   leafOffNFlux = 2.0, unclaimedStorage = 0 + 2.0 * climate->length = 0.25
  //   availableN = max(0, 0.625 + 0.25) = 0.875
  //   maxUptake = 10 * climate->length = 1.25 -> reduction = 0.875 / 1.25 = 0.7
  double minN2 = 0.625;
  double resorpFlux = 2.0;
  double demandFlux = 10.0;  // sum of demand fluxes set in initNLimitationState
  double unclaimedStorage = resorpFlux * climate->length;
  double availableN = minN2 + unclaimedStorage;
  double maxUptake = demandFlux * climate->length;
  double reduction = availableN / maxUptake;
  initNLimitationState(minN2, 0);
  fluxes.leafOffNResorption = resorpFlux;

  doNFixUpLimitCalcs();

  status |= checkNLimitationFlux(fluxes.leafCreation, 60 * reduction,
                                 "[turnover resorption] leafCreation");
  status |= checkNLimitationFlux(fluxes.woodCreation, 500 * reduction,
                                 "[turnover resorption] woodCreation");

  return status;
}

/////
// calcPoolNDemandFlux: nitrogen demand for a single biomass pool

int checkDemandFlux(double result, double expected, const char *label) {
  int status = 0;
  if (!compareDoubles(result, expected)) {
    status = 1;
    logTest("[calcPoolNDemandFlux] %s: got %8.4f, expected %8.4f\n", label,
            result, expected);
  }
  return status;
}

int testCalcPoolNDemandFlux(void) {
  int status = 0;
  logTest("Running testCalcPoolNDemandFlux\n");

  // climate->length = 0.125 (set in setupTests)
  // leafCN = 20, woodCN = 100

  // No extra N, positive creation: demand = creationCFlux / CN
  status |= checkDemandFlux(calcPoolNDemandFlux(0.0, 60.0, params.leafCN), 3.0,
                            "no extraN, leafCN");
  status |= checkDemandFlux(calcPoolNDemandFlux(0.0, 100.0, params.woodCN), 1.0,
                            "no extraN, woodCN");

  // Zero creation: demand = 0 regardless of extraN
  status |= checkDemandFlux(calcPoolNDemandFlux(0.0, 0.0, params.leafCN), 0.0,
                            "zero creation");
  status |= checkDemandFlux(calcPoolNDemandFlux(1.0, 0.0, params.leafCN), 0.0,
                            "zero creation with extraN");

  // Negative creation (e.g. loss without creation): demand = 0
  status |= checkDemandFlux(calcPoolNDemandFlux(0.0, -10.0, params.leafCN), 0.0,
                            "negative creation");

  // Partial extra N: demand reduced by extraNFlux = extraN / length
  // extraNFlux = 0.25 / 0.125 = 2.0; demandFlux = 60/20 = 3.0
  // result = max(0, 3.0 - 2.0) = 1.0
  status |= checkDemandFlux(calcPoolNDemandFlux(0.25, 60.0, params.leafCN), 1.0,
                            "partial extraN");

  // Extra N exactly covers demand: result = 0
  // extraNFlux = 0.375 / 0.125 = 3.0 = demandFlux; result = max(0, 0) = 0
  status |= checkDemandFlux(calcPoolNDemandFlux(0.375, 60.0, params.leafCN),
                            0.0, "extraN exactly covers demand");

  // Extra N exceeds demand: result clamped to 0
  // extraNFlux = 0.5 / 0.125 = 4.0 > demandFlux 3.0; result = max(0, -1.0) = 0
  status |= checkDemandFlux(calcPoolNDemandFlux(0.5, 60.0, params.leafCN), 0.0,
                            "extraN exceeds demand");

  return status;
}

/////
// updateNitrogenTrackers: verify extraN and N cycle tracker calculations

int checkTrackerVal(double actual, double expected, const char *label) {
  int status = 0;
  if (!compareDoubles(actual, expected)) {
    status = 1;
    logTest("[updateNitrogenTrackers] %s: got %8.6f, expected %8.6f\n", label,
            actual, expected);
  }
  return status;
}

int testUpdateNitrogenTrackers(void) {
  int status = 0;
  logTest("Running testUpdateNitrogenTrackers\n");

  initNitrogenTrackers();

  // Case 1: pools exactly at C/CN ratio -> all extraN == 0
  // plantLeafC=40, leafCN=20 -> expected leafN=2; set plantLeafN=2
  resetState();
  envi.plantLeafC = 40.0;
  envi.plantLeafN = 40.0 / params.leafCN;  // 2.0
  envi.plantWoodC = 100.0;
  envi.plantWoodN = 100.0 / params.woodCN;  // 1.0
  envi.fineRootC = 80.0;
  envi.fineRootN = 80.0 / params.fineRootCN;  // 2.0
  envi.coarseRootC = 50.0;
  envi.coarseRootN = 50.0 / params.woodCN;  // 0.5
  fluxes.nVolatilization = 1.0;
  fluxes.nLeaching = 0.5;
  fluxes.nFixation = 0.2;
  fluxes.nUptake = 0.3;

  updateNitrogenTrackers();

  status |=
      checkTrackerVal(nitrogenTrackers.leafExtraN, 0.0, "leafExtraN balanced");
  status |=
      checkTrackerVal(nitrogenTrackers.woodExtraN, 0.0, "woodExtraN balanced");
  status |= checkTrackerVal(nitrogenTrackers.fineRootExtraN, 0.0,
                            "fineRootExtraN balanced");
  status |= checkTrackerVal(nitrogenTrackers.coarseRootExtraN, 0.0,
                            "coarseRootExtraN balanced");
  // Trackers should capture fluxes * length
  status |= checkTrackerVal(nitrogenTrackers.n2o, 1.0 * climate->length, "n2o");
  status |= checkTrackerVal(nitrogenTrackers.nLeaching, 0.5 * climate->length,
                            "nLeaching");
  status |= checkTrackerVal(nitrogenTrackers.nFixation, 0.2 * climate->length,
                            "nFixation");
  status |= checkTrackerVal(nitrogenTrackers.nUptake, 0.3 * climate->length,
                            "nUptake");

  // Case 2: extra N in leaf pool (plantLeafN > plantLeafC/leafCN)
  // plantLeafC=40, leafCN=20 -> expected=2; set plantLeafN=2.5 -> extraN=0.5
  resetState();
  envi.plantLeafC = 40.0;
  envi.plantLeafN = 2.5;
  envi.plantWoodC = 100.0;
  envi.plantWoodN = 100.0 / params.woodCN;
  envi.fineRootC = 80.0;
  envi.fineRootN = 80.0 / params.fineRootCN;
  envi.coarseRootC = 50.0;
  envi.coarseRootN = 50.0 / params.woodCN;

  updateNitrogenTrackers();

  status |=
      checkTrackerVal(nitrogenTrackers.leafExtraN, 0.5, "leafExtraN positive");
  status |=
      checkTrackerVal(nitrogenTrackers.woodExtraN, 0.0, "woodExtraN still 0");

  // Case 3: N deficit in wood pool should be clamped to 0
  // plantWoodN < plantWoodC/woodCN -> extraN negative -> clamped to 0
  resetState();
  envi.plantLeafC = 40.0;
  envi.plantLeafN = 40.0 / params.leafCN;
  envi.plantWoodC = 100.0;
  envi.plantWoodN = 0.5;  // less than expected 1.0
  envi.fineRootC = 80.0;
  envi.fineRootN = 80.0 / params.fineRootCN;
  envi.coarseRootC = 50.0;
  envi.coarseRootN = 50.0 / params.woodCN;

  updateNitrogenTrackers();

  status |= checkTrackerVal(nitrogenTrackers.woodExtraN, 0.0,
                            "woodExtraN deficit clamped to 0");

  return status;
}

/////
// updateNitrogenPools: biomass N pool updates from creation and loss fluxes

int checkBiomassN(double actual, double expected, const char *label) {
  int status = 0;
  if (!compareDoubles(actual, expected)) {
    status = 1;
    logTest("[testBiomassNPools] %s: got %8.6f, expected %8.6f\n", label,
            actual, expected);
  }
  return status;
}

int testBiomassNPoolUpdates(void) {
  int status = 0;
  logTest("Running testBiomassNPoolUpdates\n");

  initNitrogenTrackers();

  // Case 1: wood creation and loss update plantWoodN
  // woodCreation=100/step, woodCN=100 -> woodNDemandFlux=1.0/step
  // woodLitter=50/step, woodCN=100 -> woodNLossFlux=0.5/step
  // net N change per step = (1.0 - 0.5) * 0.125 = 0.0625
  resetState();
  envi.plantWoodN = 1.0;
  envi.plantWoodC = 100.0;
  fluxes.woodCreation = 100.0;
  fluxes.woodLitter = 50.0;

  updateNitrogenPools();

  double expWoodN = 1.0 + (100.0 / params.woodCN - 50.0 / params.woodCN) *
                              climate->length;  // 1.0 + 0.0625
  status |= checkBiomassN(envi.plantWoodN, expWoodN,
                          "plantWoodN after creation/loss");

  // Case 2: leaf creation and loss update plantLeafN
  // leafCreation=40/step, leafCN=20 -> leafNDemandFlux=2.0/step
  // leafLitter=20/step, leafCN=20 -> leafNLossFlux=1.0/step
  // net N change = (2.0 - 1.0) * 0.125 = 0.125
  resetState();
  envi.plantLeafN = 0.5;
  envi.plantLeafC = 10.0;
  fluxes.leafCreation = 40.0;
  fluxes.leafLitter = 20.0;

  updateNitrogenPools();

  double expLeafN = 0.5 + (40.0 / params.leafCN - 20.0 / params.leafCN) *
                              climate->length;  // 0.5 + 0.125
  status |= checkBiomassN(envi.plantLeafN, expLeafN,
                          "plantLeafN after creation/loss");

  // Case 3: fine root creation and loss update fineRootN
  // fineRootCreation=80/step, fineRootCN=40 -> fineRootNDemandFlux=2.0/step
  // fineRootLoss=40/step, fineRootCN=40 -> fineRootNLossFlux=1.0/step
  // net N change = (2.0 - 1.0) * 0.125 = 0.125
  resetState();
  envi.fineRootN = 1.0;
  envi.fineRootC = 40.0;
  fluxes.fineRootCreation = 80.0;
  fluxes.fineRootLoss = 40.0;

  updateNitrogenPools();

  double expFineN =
      1.0 +
      (80.0 / params.fineRootCN - 40.0 / params.fineRootCN) * climate->length;
  status |=
      checkBiomassN(envi.fineRootN, expFineN, "fineRootN after creation/loss");

  // Case 4: leaf-on event redistributes N from wood/coarse root to leaf
  // leafOnCreation=20, all from wood (leafOnCreationFromWood=20)
  // leafN increases by 20/leafCN = 20/20 = 1.0 per step
  // woodN decreases by 20/woodCN = 20/100 = 0.2 per step
  // storageN decreases by calcLeafOnNFromC(20) = max(0,20/20-20/100) = 0.8 per
  // step
  resetState();
  envi.plantLeafN = 0.5;
  envi.plantWoodN = 2.0;
  envi.coarseRootN = 1.0;
  envi.plantStorageN = 1.0;  // enough for leaf-on N
  fluxes.leafOnCreation = 20.0;
  fluxes.leafOnCreationFromWood = 20.0;  // all from wood

  updateNitrogenPools();

  double leafOnLeafDelta = 20.0 / params.leafCN * climate->length;  // 0.125
  double leafOnWoodDelta = 20.0 / params.woodCN * climate->length;  // 0.025
  status |= checkBiomassN(envi.plantLeafN, 0.5 + leafOnLeafDelta,
                          "plantLeafN after leaf-on");
  status |= checkBiomassN(envi.plantWoodN, 2.0 - leafOnWoodDelta,
                          "plantWoodN after leaf-on");
  // coarseRootN unchanged since all came from wood
  status |= checkBiomassN(envi.coarseRootN, 1.0,
                          "coarseRootN unchanged after all-wood leaf-on");

  // Case 5: leaf-on from both wood and coarse root
  // leafOnCreation=20, leafOnCreationFromWood=10, rest from root
  // leafN += 20/20 * dt = 1.0 * 0.125 = 0.125
  // woodN -= 10/100 * dt = 0.1 * 0.125 = 0.0125
  // coarseRootN -= 10/100 * dt = 0.1 * 0.125 = 0.0125
  resetState();
  envi.plantLeafN = 0.5;
  envi.plantWoodN = 2.0;
  envi.coarseRootN = 1.0;
  envi.plantStorageN = 1.0;
  fluxes.leafOnCreation = 20.0;
  fluxes.leafOnCreationFromWood = 10.0;

  updateNitrogenPools();

  double leafOnLeafDelta2 = 20.0 / params.leafCN * climate->length;  // 0.125
  double leafOnWoodDelta2 = 10.0 / params.woodCN * climate->length;  // 0.0125
  double leafOnRootDelta2 = 10.0 / params.woodCN * climate->length;  // 0.0125
  status |= checkBiomassN(envi.plantLeafN, 0.5 + leafOnLeafDelta2,
                          "plantLeafN after split leaf-on");
  status |= checkBiomassN(envi.plantWoodN, 2.0 - leafOnWoodDelta2,
                          "plantWoodN after split leaf-on");
  status |= checkBiomassN(envi.coarseRootN, 1.0 - leafOnRootDelta2,
                          "coarseRootN after split leaf-on");

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
  status |= testCalcPoolNDemandFlux();
  status |= testUpdateNitrogenTrackers();
  status |= testBiomassNPoolUpdates();

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

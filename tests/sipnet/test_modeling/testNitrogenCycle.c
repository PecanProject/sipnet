#include "utils/tUtils.h"
#include "sipnet/events.c"
#include "sipnet/sipnet.c"

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

  // Volatilization
  params.nVolatilizationFrac = 0;
  fluxes.nVolatilization = 0;
  // Leaching
  params.nLeachingFrac = 0;
  fluxes.nLeaching = 0;
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

  // Initialize general state
  initGeneralState();
}

int checkFlux(double calcFlux, double expFlux, const char *label) {
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
  double mEffect = calcMoistEffect(envi.soilWater, params.soilWHC);
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
  initEvents("events_fert.in", 0);
  setupEvents();

  calcNVolatilizationFlux();
  processEvents();
  updateNitrogenPools();
  updatePoolsForEvents();

  // Want to test:
  // envi: minN
  // fluxes: nVol, eventMinN
  double tEffect = calcTempEffect(climate->tsoil);
  double mEffect = calcMoistEffect(envi.soilWater, params.soilWHC);
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

int run(void) {
  int status = 0;

  setupTests();

  status |= testNVolatilization();
  status |= testFertilization();
  status |= testNLeaching();
  status |= testOrganicN();

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

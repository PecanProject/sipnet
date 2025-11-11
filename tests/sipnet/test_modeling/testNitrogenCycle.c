#include <stdio.h>

#include "utils/tUtils.h"
#include "sipnet/events.c"
#include "sipnet/sipnet.c"

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
}

void initState(double initN, double nVol, double nLeachFrac) {
  // per test
  envi.minN = initN;
  params.nVolatilization = nVol;
  params.nLeachingFrac = nLeachFrac;

  // static
  envi.soilWater = 5.0;
  envi.soil = 1.5;
  envi.litter = 1;

  params.minNInit = 0.0;
  params.soilWHC = 10.0;

  fluxes.nVolatilization = 0;
  fluxes.nLeaching = 0;

  // Values from russell_2 smoke test
  params.soilRespMoistEffect = 1.0;
  params.baseSoilResp = 0.06;
  params.soilRespQ10 = 2.9;
}

int checkFlux(double expNVolFlux) {
  int status = 0;
  if (!compareDoubles(fluxes.nVolatilization, expNVolFlux)) {
    status = 1;
    logTest("N volatilization flux is %f, expected %f\n",
            fluxes.nVolatilization, expNVolFlux);
  }

  return status;
}

int checkNLeachingFlux(double expNLeachingFlux) {
  int status = 0;
  if (!compareDoubles(fluxes.nLeaching, expNLeachingFlux)) {
    status = 1;
    logTest("N leaching flux is %f, expected %f\n", fluxes.nLeaching,
            expNLeachingFlux);
  }

  return status;
}

int testNLeaching(void) {
  int status = 0;
  double minN;
  double nLeachFrac;
  double expNLeaching;
  double phi;

  minN = 1;
  nLeachFrac = 0.5;
  fluxes.drainage = 5;
  initState(minN, 0, nLeachFrac);
  if ((fluxes.drainage / params.soilWHC) < 1) {
    phi = fluxes.drainage / params.soilWHC;
  } else {
    phi = 1;
  }
  expNLeaching = minN * phi * nLeachFrac;
  calcNLeachingFlux();
  status |= checkNLeachingFlux(expNLeaching);

  minN = 1;
  nLeachFrac = 0.5;
  fluxes.drainage = 20;
  initState(minN, 0, nLeachFrac);
  if ((fluxes.drainage / params.soilWHC) < 1) {
    phi = fluxes.drainage / params.soilWHC;
  } else {
    phi = 1;
  }
  expNLeaching = minN * phi * nLeachFrac;
  calcNLeachingFlux();
  status |= checkNLeachingFlux(expNLeaching);

  // Check minN for the last
  updatePoolsForSoil();
  double expMinN = minN - (expNLeaching * climate->length);
  int minStatus = 0;
  if (!compareDoubles(envi.minN, expMinN)) {
    minStatus = 1;
    logTest("minN pool is %8.3f, expected %8.3f\n", envi.minN, expMinN);
  }
  status |= minStatus;

  return status;
}

int testNVol(void) {
  int status = 0;
  double minN;
  double nVol;
  double expNVol;

  minN = 2;
  nVol = 0.1;
  initState(minN, nVol, 0);
  double tEffect = calcTempEffect(climate->tsoil);
  double mEffect = calcMoistEffect(envi.soilWater, params.soilWHC);
  expNVol = (nVol * minN * tEffect * mEffect) / climate->length;
  calcNVolatilizationFlux();
  status |= checkFlux(expNVol);

  // easy proportionality test - doubling params should double output
  minN *= 2;
  expNVol *= 2;
  initState(minN, nVol, 0);
  calcNVolatilizationFlux();
  status |= checkFlux(expNVol);

  expNVol *= 2;
  initState(minN, nVol, 0);
  params.baseSoilResp *= 2;  // should double tEffect
  calcNVolatilizationFlux();
  status |= checkFlux(expNVol);

  // Check minN for the last
  updatePoolsForSoil();
  double expMinN = minN - (expNVol * climate->length);
  int minStatus = 0;
  if (!compareDoubles(envi.minN, expMinN)) {
    minStatus = 1;
    logTest("minN pool is %8.3f, expected %8.3f\n", envi.minN, expMinN);
  }
  status |= minStatus;

  return status;
}

int testFert(void) {
  int status = 0;
  double initN = 2.0;
  double nVolFrac = 0.1;
  double expMinN, expEventMinN, expNVol;

  // init minN 2, nVol 0.1
  initState(initN, nVolFrac, 0);

  // fert event: 15 5 10
  double fertMinN = 10;
  initEvents("events_fert.in", 0);
  setupEvents();

  calcNVolatilizationFlux();
  processEvents();
  updatePoolsForSoil();
  updatePoolsForEvents();

  // Want to test:
  // envi: minN
  // fluxes: nVol, eventMinN
  double tEffect = calcTempEffect(climate->tsoil);
  double mEffect = calcMoistEffect(envi.soilWater, params.soilWHC);
  expNVol = (nVolFrac * initN * tEffect * mEffect) / climate->length;
  expEventMinN = fertMinN / climate->length;
  expMinN = initN + (expEventMinN - expNVol) * climate->length;

  if (!compareDoubles(fluxes.nVolatilization, expNVol)) {
    status = 1;
    logTest("fluxes.nVolatilization is %8.4f, expected %8.4f\n",
            fluxes.nVolatilization, expNVol);
  }
  if (!compareDoubles(fluxes.eventMinN, expEventMinN)) {
    status = 1;
    logTest("fluxes.eventMinN is %8.4f, expected %8.4f\n", fluxes.eventMinN,
            expEventMinN);
  }
  if (!compareDoubles(envi.minN, expMinN)) {
    status = 1;
    logTest("envi.minN is %8.4f, expected %8.4f\n", envi.minN, expMinN);
  }

  return status;
}

int run(void) {
  int status = 0;

  setupTests();

  status |= testFert();

  status |= testNVol();

  status |= testNLeaching();

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

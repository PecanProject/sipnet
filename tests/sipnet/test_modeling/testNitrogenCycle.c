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

void initState(double initN, double nVol) {
  // per test
  envi.minN = initN;
  params.nVolatilizationFrac = nVol;

  // static
  envi.soilWater = 5.0;
  envi.soil = 1.5;
  envi.litter = 1;

  params.minNInit = 0.0;
  params.soilWHC = 10.0;

  fluxes.nVolatilization = 0;

  // Values from russell_2 smoke test
  params.soilRespMoistEffect = 1.0;
  params.baseSoilResp = 0.06;
  params.soilRespQ10 = 2.9;
}

int checkVolatilizationFlux(double expNVolFlux) {
  int status = 0;
  if (!compareDoubles(fluxes.nVolatilization, expNVolFlux)) {
    status = 1;
    logTest("N volatilization flux is %f, expected %f\n",
            fluxes.nVolatilization, expNVolFlux);
  }

  return status;
}

int testNVolatilization(void) {
  int status = 0;
  double minN;
  double nVolFrac;
  double expNVolFlux;

  minN = 2;
  nVolFrac = 0.1;
  initState(minN, nVolFrac);
  double tEffect = calcTempEffect(climate->tsoil);
  double mEffect = calcMoistEffect(envi.soilWater, params.soilWHC);
  expNVolFlux = nVolFrac * minN * tEffect * mEffect;
  calcNVolatilizationFlux();
  status |= checkVolatilizationFlux(expNVolFlux);

  // easy proportionality test - doubling params should double output
  minN *= 2;
  expNVolFlux *= 2;
  initState(minN, nVolFrac);
  calcNVolatilizationFlux();
  status |= checkVolatilizationFlux(expNVolFlux);

  // Check minN for the last
  updatePoolsForSoil();
  double expMinN = minN - expNVolFlux * climate->length;
  int minStatus = 0;
  if (!compareDoubles(envi.minN, expMinN)) {
    minStatus = 1;
    logTest("minN pool is %8.3f, expected %8.3f\n", envi.minN, expMinN);
  }
  status |= minStatus;

  return status;
}

int testFertilization(void) {
  int status = 0;
  double initN = 2.0;
  double nVolFrac = 0.1;
  double expMinN, expEventMinNFlux, expNVolFlux;

  // init minN 2, nVol 0.1
  initState(initN, nVolFrac);

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

int run(void) {
  int status = 0;

  setupTests();

  status |= testFertilization();

  status |= testNVolatilization();

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

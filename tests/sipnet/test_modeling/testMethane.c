#include "utils/tUtils.h"
#include "sipnet/sipnet.c"

void resetState(void) {
  ctx.litterPool = 1;
  ctx.anaerobic = 1;

  envi.soilWater = 7.5;

  params.soilMethaneRate = 0.05;
  params.litterMethaneRate = 0.1;
}

void setupTests(void) {
  // set up dummy climate
  climate = (ClimateNode *)malloc(sizeof(ClimateNode));
  climate->year = 2024;
  climate->day = 70;
  climate->length = 0.125;
  climate->tsoil = 20.0;

  // Environment
  envi.soilWater = 7.5;
  // soil CN 7.5
  envi.soilC = 15.0;
  envi.soilOrgN = 2.0;
  // litter CN 5.0
  envi.litterC = 7.5;
  envi.litterN = 1.5;

  params.soilWHC = 10.0;
  params.soilRespQ10 = 3.0;
  params.fAnoxia = 0.6;
  params.anaerobicDecompRate = 0.5;
  params.anaerobicTransExp = 2.0;

  // Initialize general state
  resetState();

  // Set up the context
  initContext();
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

int checkPool(double calcPool, double expPool, const char *label) {
  // Make sure we didn't forget to update context, in case dependencies changed
  validateContext();

  int status = 0;
  if (!compareDoubles(calcPool, expPool)) {
    status = 1;
    logTest("Calculated %s pool is %f, expected %f\n", label, calcPool,
            expPool);
  }

  return status;
}

int checkSoil(double flux, double pool) {
  int status = 0;
  status |= checkFlux(fluxes.soilMethane, flux, "soil");
  status |= checkPool(envi.soilC, pool, "soil");
  return status;
}

int checkLitter(double flux, double pool) {
  int status = 0;
  status |= checkFlux(fluxes.litterMethane, flux, "litter");
  status |= checkPool(envi.litterC, pool, "litter");
  return status;
}

void callCalcMethaneFlux(void) {
  // Make sure we didn't make a dumb mistake with tests points
  // That is, make sure we HAVE some methane flux
  if (envi.soilWater / params.soilWHC <= params.fAnoxia + 0.1) {
    logTest("Soil water is not high enough to trigger methane production!\n");
    logTest("SoilW %8.4f WHC %8.4f f_a %8.4f\n", envi.soilWater, params.soilWHC,
            params.fAnoxia);
    exit(1);
  }

  calcMethaneFlux();
}

int testMethane(void) {
  int status = 0;
  double expSoilF, expSoilC;
  double expLitterF, expLitterC;

  double tempEffect = calcTempEffect(climate->tsoil);
  double moistEfffect = calcMethaneMoistEffect(envi.soilWater, params.soilWHC);

  // Standard
  logTest("  Test: standard\n");
  resetState();
  callCalcMethaneFlux();
  updatePoolsForSoil();
  expSoilF = 0.75 * tempEffect * moistEfffect;
  expLitterF = 0.75 * tempEffect * moistEfffect;
  expSoilC = 15 - expSoilF * climate->length;
  expLitterC = 7.5 - expLitterF * climate->length;
  status |= checkSoil(expSoilF, expSoilC);
  status |= checkLitter(expLitterF, expLitterC);

  // No litter pool
  logTest("  Test: no litter pool\n");
  resetState();
  ctx.litterPool = 0;
  envi.soilC = 20.0;
  envi.litterC = 0.0;
  callCalcMethaneFlux();
  updatePoolsForSoil();
  expSoilF = 1.0 * tempEffect * moistEfffect;
  expLitterF = 0.0;
  expSoilC = 20.0 - expSoilF * climate->length;
  expLitterC = 0.0;
  status |= checkSoil(expSoilF, expSoilC);
  status |= checkLitter(expLitterF, expLitterC);
  return status;
}

int run(void) {
  int status = 0;

  setupTests();

  status |= testMethane();

  return status;
}

int main(void) {
  int status;

  logTest("Starting testMethane:run()\n");
  status = run();
  if (status) {
    logTest("FAILED testMethane with status %d\n", status);
    exit(status);
  }

  logTest("PASSED testMethane\n");
}

#include "utils/tUtils.h"
#include "sipnet/sipnet.c"

void resetState(void) {
  ctx.litterPool = 1;
  ctx.carbonSaturation = 1;

  envi.litterC = 10;

  params.soilCSaturation = 10;

  fluxes.woodLitter = 0;
  fluxes.leafLitter = 0;
  fluxes.litterToSoil = 0;
  fluxes.rLitter = 0;
  fluxes.litterMethane = 0;
  fluxes.rSoil = 0;
  fluxes.soilMethane = 0;
  fluxes.fineRootLoss = 0;
}

void setupTests(void) {
  // set up dummy climate
  climate = (ClimateNode *)malloc(sizeof(ClimateNode));
  climate->year = 2024;
  climate->day = 70;
  climate->length = 0.125;
  climate->tsoil = 20.0;

  // Initialize general state
  resetState();

  // Set up the context
  initContext();
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

int checkSoil(double pool) {
  int status = 0;
  status |= checkPool(envi.soilC, pool, "soil");
  return status;
}

int checkLitter(double pool) {
  int status = 0;
  status |= checkPool(envi.litterC, pool, "litter");
  return status;
}

int testCarbonSaturation(void) {
  int status = 0;
  double expSoilC;
  double expLitterC;
  double saturationFraction;
  double soilInputs;

  // Low C inputs, low saturationFraction
  logTest("  Test: low soilInputs, low satFrac\n");
  resetState();

  expSoilC = 2.5;  // 25% saturation
  envi.soilC = 2.5;
  expLitterC = 10;
  fluxes.coarseRootLoss = 100;

  soilInputs =
      fluxes.coarseRootLoss + fluxes.fineRootLoss + fluxes.litterToSoil;
  saturationFraction = expSoilC / params.soilCSaturation;

  expLitterC += (fluxes.woodLitter + fluxes.leafLitter +
                 (soilInputs * saturationFraction) - fluxes.litterToSoil -
                 fluxes.rLitter - fluxes.litterMethane) *
                climate->length;

  expSoilC += (soilInputs * (1 - saturationFraction) - fluxes.rSoil -
               fluxes.soilMethane) *
              climate->length;

  updatePoolsForSoil();
  status |= checkSoil(expSoilC);
  status |= checkLitter(expLitterC);

  // High C inputs, low saturationFraction
  logTest("  Test: high soilInputs, low satFrac\n");
  resetState();

  expSoilC = 2.5;  // 25% saturation
  envi.soilC = 2.5;
  expLitterC = 10;
  fluxes.coarseRootLoss = 200;

  soilInputs =
      fluxes.coarseRootLoss + fluxes.fineRootLoss + fluxes.litterToSoil;
  saturationFraction = expSoilC / params.soilCSaturation;

  expLitterC += (fluxes.woodLitter + fluxes.leafLitter +
                 (soilInputs * saturationFraction) - fluxes.litterToSoil -
                 fluxes.rLitter - fluxes.litterMethane) *
                climate->length;

  expSoilC += (soilInputs * (1 - saturationFraction) - fluxes.rSoil -
               fluxes.soilMethane) *
              climate->length;

  updatePoolsForSoil();
  status |= checkSoil(expSoilC);
  status |= checkLitter(expLitterC);

  // Low C inputs, high saturationFraction
  logTest("  Test: low soilInputs, high satFrac\n");
  resetState();

  expSoilC = 7.5;  // 75% saturation
  envi.soilC = 7.5;
  expLitterC = 10;
  fluxes.coarseRootLoss = 100;

  soilInputs =
      fluxes.coarseRootLoss + fluxes.fineRootLoss + fluxes.litterToSoil;
  saturationFraction = expSoilC / params.soilCSaturation;

  expLitterC += (fluxes.woodLitter + fluxes.leafLitter +
                 (soilInputs * saturationFraction) - fluxes.litterToSoil -
                 fluxes.rLitter - fluxes.litterMethane) *
                climate->length;

  expSoilC += (soilInputs * (1 - saturationFraction) - fluxes.rSoil -
               fluxes.soilMethane) *
              climate->length;

  updatePoolsForSoil();
  status |= checkSoil(expSoilC);
  status |= checkLitter(expLitterC);

  // High C inputs, high saturationFraction
  logTest("  Test: high soilInputs, high satFrac\n");
  resetState();

  expSoilC = 7.5;  // 75% saturation
  envi.soilC = 7.5;
  expLitterC = 10;
  fluxes.coarseRootLoss = 200;

  soilInputs =
      fluxes.coarseRootLoss + fluxes.fineRootLoss + fluxes.litterToSoil;
  saturationFraction = expSoilC / params.soilCSaturation;

  expLitterC += (fluxes.woodLitter + fluxes.leafLitter +
                 (soilInputs * saturationFraction) - fluxes.litterToSoil -
                 fluxes.rLitter - fluxes.litterMethane) *
                climate->length;

  expSoilC += (soilInputs * (1 - saturationFraction) - fluxes.rSoil -
               fluxes.soilMethane) *
              climate->length;

  updatePoolsForSoil();
  status |= checkSoil(expSoilC);
  status |= checkLitter(expLitterC);

  return status;
}

int run(void) {
  int status = 0;

  setupTests();

  status |= testCarbonSaturation();

  return status;
}

int main(void) {
  int status;

  logTest("Starting testCarbonSaturation:run()\n");
  status = run();
  if (status) {
    logTest("FAILED testCarbonSaturation with status %d\n", status);
    exit(status);
  }

  logTest("PASSED testCarbonSaturation\n");
}

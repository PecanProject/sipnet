#include "utils/tUtils.h"
#include "sipnet/sipnet.c"

void setupTests(void) {
  // Set up dummy climate
  climate = (ClimateNode *)malloc(sizeof(ClimateNode));
  climate->year = 2024;
  climate->day = 70;
  climate->length = 0.125;
  climate->tsoil = 20.0;
  climate->wspd = 2.0;
  climate->vpdSoil = 0.5;

  // Set up the context
  initContext();

  // Params
  params.soilWHC = 10.0;
  params.fastFlowFrac = 0.0;
  params.waterDrainFrac = 1.0;
  params.waterRemoveFrac = 0.5;
  params.frozenSoilThreshold = -30.0;
  params.frozenSoilEff = 1.0;
  params.wueConst = 10.0;
  params.rdConst = 100.0;
  params.rSoilConst1 = 3.5;
  params.rSoilConst2 = 10.0;

  // Environment
  envi.snow = 0;
  envi.soilWater = 5.0;
}

int checkValue(double calc, double exp, const char *label) {
  // Make sure we didn't forget to update context, in case dependencies changed
  validateContext();

  int status = 0;
  if (!compareDoubles(calc, exp)) {
    status = 1;
    logTest("  %s: calculated %f, expected %f\n", label, calc, exp);
  }
  return status;
}

/////
// Test getClippedWaterFrac: bounded in [0, 1]
int testGetClippedWaterFrac(void) {
  logTest("Running testGetClippedWaterFrac\n");
  int status = 0;

  // Normal: within range
  status |= checkValue(getClippedWaterFrac(5.0, 10.0), 0.5, "half WHC");
  // At upper bound
  status |= checkValue(getClippedWaterFrac(10.0, 10.0), 1.0, "at WHC");
  // Above WHC (flooded soil): clips to 1.0
  status |= checkValue(getClippedWaterFrac(15.0, 10.0), 1.0, "above WHC");
  // At zero
  status |= checkValue(getClippedWaterFrac(0.0, 10.0), 0.0, "at zero");
  // Below zero: clips to 0.0
  status |= checkValue(getClippedWaterFrac(-1.0, 10.0), 0.0, "below zero");

  return status;
}

/////
// Test drainage in calcSoilWaterFluxes with various waterDrainFrac values
//
// Setup: snow present (skips evaporation), no netRain/snowMelt/trans, so
// waterRemaining = water. Drainage triggers when water > soilWHC.
int testDrainageWithWaterDrainFrac(void) {
  logTest("Running testDrainageWithWaterDrainFrac\n");
  int status = 0;
  double fastFlow, evaporation, drainage;

  double water = 12.0;  // 2 cm above soilWHC=10
  double excessOverLen = (water - params.soilWHC) / climate->length;

  // Use snow to suppress evaporation so drainage calc is straightforward
  envi.snow = 1.0;

  // waterDrainFrac = 1.0: all excess drains immediately
  params.waterDrainFrac = 1.0;
  calcSoilWaterFluxes(&fastFlow, &evaporation, &drainage, water, 0, 0, 0);
  status |= checkValue(drainage, excessOverLen * 1.0, "drainage (frac=1.0)");

  // waterDrainFrac = 0.5: half of excess drains per step
  params.waterDrainFrac = 0.5;
  calcSoilWaterFluxes(&fastFlow, &evaporation, &drainage, water, 0, 0, 0);
  status |= checkValue(drainage, excessOverLen * 0.5, "drainage (frac=0.5)");

  // waterDrainFrac = 0.0: no drainage (water stays above WHC, modeling
  // flooding)
  params.waterDrainFrac = 0.0;
  calcSoilWaterFluxes(&fastFlow, &evaporation, &drainage, water, 0, 0, 0);
  status |= checkValue(drainage, 0.0, "drainage (frac=0.0)");

  // No drainage when water is at or below soilWHC
  params.waterDrainFrac = 1.0;
  calcSoilWaterFluxes(&fastFlow, &evaporation, &drainage, params.soilWHC, 0, 0,
                      0);
  status |= checkValue(drainage, 0.0, "no drainage at WHC");

  envi.snow = 0;
  return status;
}

/////
// Test moisture(): transpiration should be capped at soilWHC when flooded
//
// With soilWater > soilWHC, removableWater = min(soilWater, soilWHC) *
// waterRemoveFrac, which equals soilWHC * waterRemoveFrac. The result is
// the same as soilWater == soilWHC.
//
// Use a large potGrossPsn so that transpiration is water-limited (not
// photosynthesis-limited) in the dry case. With soilWHC=10 and
// waterRemoveFrac=0.5:
//   removableWater at dry (20% WHC) = 0.2*10*0.5 = 1.0 cm/day
//   removableWater at WHC           = 10*0.5      = 5.0 cm/day
//   potTrans (potGrossPsn=50, vpd=1, wueConst=10) ≈ 1.83 cm/day
// So: dry is water-limited (trans=1.0), at-WHC/flooded is psn-limited (trans
// ≈1.83).
int testMoistureFloodedSoil(void) {
  logTest("Running testMoistureFloodedSoil\n");
  int status = 0;
  double transAtWHC, dWaterAtWHC;
  double trans, dWater;

  // Use a large potGrossPsn so that water is the limiting factor for dry soil
  double potGrossPsn = 50.0;
  double vpd = 1.0;

  // Baseline: soilWater exactly at soilWHC
  moisture(&transAtWHC, &dWaterAtWHC, potGrossPsn, vpd, params.soilWHC);

  // Flooded: soilWater double the soilWHC; should produce same result
  moisture(&trans, &dWater, potGrossPsn, vpd, 2 * params.soilWHC);
  status |= checkValue(trans, transAtWHC, "trans same when flooded vs. at WHC");
  status |=
      checkValue(dWater, dWaterAtWHC, "dWater same when flooded vs. at WHC");

  // Dry soil: soilWater well below soilWHC; trans should differ (water-limited)
  double transLow, dWaterLow;
  moisture(&transLow, &dWaterLow, potGrossPsn, vpd, params.soilWHC * 0.2);
  if (compareDoubles(transLow, transAtWHC)) {
    status = 1;
    logTest("  trans should be lower for dry soil than at WHC\n");
  }

  return status;
}

int run(void) {
  int status = 0;

  setupTests();

  status |= testGetClippedWaterFrac();
  status |= testDrainageWithWaterDrainFrac();
  status |= testMoistureFloodedSoil();

  return status;
}

int main(void) {
  int status;

  logTest("Starting testSoilMoisture:run()\n");
  status = run();
  if (status) {
    logTest("FAILED testSoilMoisture with status %d\n", status);
    exit(status);
  }

  logTest("PASSED testSoilMoisture\n");
  return 0;
}

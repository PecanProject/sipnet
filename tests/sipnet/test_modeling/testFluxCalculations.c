#include "utils/tUtils.h"
#include "sipnet/events.c"
#include "sipnet/sipnet.c"

/////
// Setup and state management

void resetContext(void) {
  ctx.litterPool = 0;
  ctx.nitrogenCycle = 0;
  ctx.events = 0;
  // waterHResp is on by default, no tests turn it off
  ctx.anaerobic = 0;
}

void setupTests(void) {
  // Set up dummy climate
  climate = (ClimateNode *)malloc(sizeof(ClimateNode));
  climate->year = 2024;
  climate->day = 70;
  climate->time = 0.0;
  climate->tsoil = 20.0;
  climate->length = 0.125;

  // Set up the context
  initContext();
  resetContext();

  // Initialize mean NPP tracker
  meanNPP = newMeanTracker(0, MEAN_NPP_DAYS, MEAN_NPP_MAX_ENTRIES);

  // Initialize event trackers
  initEventTrackers();
}

void resetFluxVars(void) { fluxes = (struct FluxVars){0}; }

int checkFlux(double calc, double exp, const char *label) {
  validateContext();
  int status = 0;
  if (!compareDoubles(calc, exp)) {
    status = 1;
    logTest("Flux %s is %f, expected %f\n", label, calc, exp);
  }
  return status;
}

/////
// calcWoodAndLeafFluxes tests

int testWoodAndLeafFluxesPositiveNPP(void) {
  int status = 0;
  logTest("Running testWoodAndLeafFluxesPositiveNPP\n");

  resetContext();
  resetFluxVars();
  // Set NPP mean to 10.0 g C/m^2/day
  resetMeanTracker(meanNPP, 10.0);

  envi.plantWoodC = 5.0;
  envi.plantCAccountingDelta = 0.0;
  envi.plantLeafC = 3.0;
  params.woodTurnoverRate = 0.1;
  params.leafTurnoverRate = 0.2;
  params.leafAllocation = 0.3;
  params.woodAllocation = 0.4;

  // Expected:
  // woodLitter = (5.0 + 0.0) * 0.1 = 0.5
  // leafLitter = 3.0 * 0.2 = 0.6
  // leafCreation = 10.0 * 0.3 = 3.0
  // woodCreation = 10.0 * 0.4 = 4.0
  // leafDeficit = 3.0 / 0.125 + 3.0 - 0.6 = 26.4 > 0 -> no adjustment
  calcWoodAndLeafFluxes();

  status |= checkFlux(fluxes.woodLitter, 0.5, "woodLitter (positive NPP)");
  status |= checkFlux(fluxes.leafLitter, 0.6, "leafLitter (positive NPP)");
  status |= checkFlux(fluxes.leafCreation, 3.0, "leafCreation (positive NPP)");
  status |= checkFlux(fluxes.woodCreation, 4.0, "woodCreation (positive NPP)");
  // No N resorption when NPP is positive
  status |= checkFlux(fluxes.reductionNResorption, 0.0,
                      "reductionNResorption (positive NPP)");
  return status;
}

int testWoodAndLeafFluxesNegativeNPP(void) {
  int status = 0;
  logTest("Running testWoodAndLeafFluxesNegativeNPP\n");

  resetContext();
  ctx.nitrogenCycle = 1;
  ctx.litterPool = 1;
  ctx.anaerobic = 1;
  resetFluxVars();
  // Set NPP mean to -2.0 (carbon loss)
  resetMeanTracker(meanNPP, -2.0);

  envi.plantWoodC = 5.0;
  envi.plantCAccountingDelta = 0.0;
  envi.plantLeafC = 3.0;
  params.woodTurnoverRate = 0.1;
  params.leafTurnoverRate = 0.2;
  params.leafAllocation = 0.3;
  params.woodAllocation = 0.4;
  params.leafCN = 20.0;
  params.woodCN = 100.0;
  params.fineRootCN = 40.0;

  // Expected:
  // woodLitter = 5.0 * 0.1 = 0.5
  // leafLitter = 3.0 * 0.2 = 0.6
  // leafCreation = -2.0 * 0.3 = -0.6
  // woodCreation = -2.0 * 0.4 = -0.8
  // leafDeficit = 3.0/0.125 + (-0.6) - 0.6 = 24 - 1.2 = 22.8 > 0 -> no
  // adjustment npp < 0:
  //   leafOffNResorption += -(-0.6) / 20.0 = 0.03
  //   leafOffNResorption += -(-0.8) / 100.0 = 0.008
  //   total = 0.038
  calcWoodAndLeafFluxes();
  checkCarbonLimitations();
  calcNResorptionFluxes();

  status |= checkFlux(fluxes.woodLitter, 0.5, "woodLitter (negative NPP)");
  status |= checkFlux(fluxes.leafLitter, 0.6, "leafLitter (negative NPP)");
  status |= checkFlux(fluxes.leafCreation, -0.6, "leafCreation (negative NPP)");
  status |= checkFlux(fluxes.woodCreation, -0.8, "woodCreation (negative NPP)");
  status |= checkFlux(fluxes.reductionNResorption, 0.038,
                      "reductionNResorption (negative NPP)");
  return status;
}

int testWoodAndLeafFluxesLeafDeficit(void) {
  int status = 0;
  logTest("Running testWoodAndLeafFluxesLeafDeficit\n");

  resetContext();
  resetFluxVars();
  // Very negative NPP causes leaf deficit - excess deducted from wood
  resetMeanTracker(meanNPP, -5.0);

  envi.plantWoodC = 10.0;
  envi.plantCAccountingDelta = 0.0;
  envi.plantLeafC = 0.0;  // empty leaf pool
  params.woodTurnoverRate = 0.0;
  params.leafTurnoverRate = 0.0;
  params.leafAllocation = 0.3;
  params.woodAllocation = 0.4;

  // Expected:
  // woodLitter = 0, leafLitter = 0
  // leafCreation = -5.0 * 0.3 = -1.5
  // woodCreation = -5.0 * 0.4 = -2.0
  // leafDeficit = 0/0.125 + (-1.5) - 0 = -1.5 < 0
  //   -> woodCreation += -1.5 -> woodCreation = -3.5
  //   -> leafCreation -= -1.5 -> leafCreation = 0.0
  calcWoodAndLeafFluxes();
  checkCarbonLimitations();

  status |= checkFlux(fluxes.leafCreation, 0.0, "leafCreation (leaf deficit)");
  status |= checkFlux(fluxes.woodCreation, -3.5, "woodCreation (leaf deficit)");
  return status;
}

int testWoodAndLeafFluxesWithAccountingDelta(void) {
  int status = 0;
  logTest("Running testWoodAndLeafFluxesWithAccountingDelta\n");

  resetContext();
  resetFluxVars();
  resetMeanTracker(meanNPP, 0.0);  // zero NPP

  envi.plantWoodC = 5.0;
  envi.plantCAccountingDelta = 3.0;  // positive accounting delta
  envi.plantLeafC = 0.0;
  params.woodTurnoverRate = 0.1;
  params.leafTurnoverRate = 0.0;
  params.leafAllocation = 0.3;
  params.woodAllocation = 0.4;

  // Wood litter uses total wood = woodC + delta = 5 + 3 = 8
  // Expected: woodLitter = 8.0 * 0.1 = 0.8
  calcWoodAndLeafFluxes();
  checkCarbonLimitations();

  status |= checkFlux(fluxes.woodLitter, 0.8, "woodLitter (with delta)");
  return status;
}

/////
// calcRootFluxes tests

int testRootFluxesPositiveNPP(void) {
  int status = 0;
  logTest("Running testRootFluxesPositiveNPP\n");

  resetContext();
  resetFluxVars();
  resetMeanTracker(meanNPP, 8.0);

  envi.coarseRootC = 5.0;
  envi.fineRootC = 3.0;
  params.coarseRootAllocation = 0.1;
  params.fineRootAllocation = 0.2;
  params.coarseRootTurnoverRate = 0.01;
  params.fineRootTurnoverRate = 0.02;
  // These are needed for calcRootResp, set to zero to avoid side effects
  params.baseCoarseRootResp = 0.0;
  params.baseFineRootResp = 0.0;

  // Expected:
  // coarseRootCreation = 8.0 * 0.1 = 0.8
  // fineRootCreation = 8.0 * 0.2 = 1.6
  // coarseRootLoss = 5.0 * 0.01 = 0.05
  // fineRootLoss = 3.0 * 0.02 = 0.06
  calcRootFluxes();
  checkCarbonLimitations();

  status |= checkFlux(fluxes.coarseRootCreation, 0.8,
                      "coarseRootCreation (positive NPP)");
  status |= checkFlux(fluxes.fineRootCreation, 1.6,
                      "fineRootCreation (positive NPP)");
  status |=
      checkFlux(fluxes.coarseRootLoss, 0.05, "coarseRootLoss (positive NPP)");
  status |= checkFlux(fluxes.fineRootLoss, 0.06, "fineRootLoss (positive NPP)");
  // No N resorption when NPP is positive
  status |= checkFlux(fluxes.reductionNResorption, 0.0,
                      "reductionNResorption (positive NPP roots)");
  return status;
}

void initRootFluxesNegativeNPPState(double nppInit, double coarseInit,
                                    double fineInit) {
  resetContext();
  ctx.nitrogenCycle = 1;
  ctx.litterPool = 1;
  ctx.anaerobic = 1;
  resetFluxVars();
  resetMeanTracker(meanNPP, nppInit);

  envi.coarseRootC = coarseInit;
  envi.fineRootC = fineInit;
  params.coarseRootAllocation = 0.1;
  params.fineRootAllocation = 0.2;
  params.coarseRootTurnoverRate = 0.0;
  params.fineRootTurnoverRate = 0.0;
  params.woodCN = 100.0;
  params.leafCN = 20.0;
  params.fineRootCN = 40.0;
  params.baseCoarseRootResp = 0.0;
  params.baseFineRootResp = 0.0;
}

int testRootFluxesNegativeNPP(void) {
  int status = 0;
  logTest("Running testRootFluxesNegativeNPP\n");

  // Case 1:
  // Expected:
  // coarseRootCreation = -4.0 * 0.1 = -0.4
  // fineRootCreation = -4.0 * 0.2 = -0.8
  // npp < 0:
  //   leafOffNResorption += -(-0.4) / 100.0 = 0.004
  //   leafOffNResorption += -(-0.8) / 40.0  = 0.02
  //   total = 0.024
  initRootFluxesNegativeNPPState(-4.0, 5.0, 3.0);
  calcRootFluxes();
  checkCarbonLimitations();
  calcNResorptionFluxes();

  status |= checkFlux(fluxes.coarseRootCreation, -0.4,
                      "coarseRootCreation (negative NPP)");
  status |= checkFlux(fluxes.fineRootCreation, -0.8,
                      "fineRootCreation (negative NPP)");
  status |= checkFlux(fluxes.reductionNResorption, 0.024,
                      "reductionNResorption (negative NPP roots)");

  // Case 2: negative fine root
  // Expected:
  // coarseRootCreation = -32.0 * 0.1 = -3.2 (-2.4) = -5.6
  // fineRootCreation = -32.0 * 0.2 = -6.4 (+2.4) = -4.0
  // [envi.FineRootC = 0.5*8 = 4; 2.4 moves to coarse]
  // npp < 0:
  //   leafOffNResorption += 5.6 / 100.0 = 0.056
  //   leafOffNResorption += 4.0 / 40.0  = 0.1
  //   total = 0.156
  initRootFluxesNegativeNPPState(-32.0, 5.0, 0.5);
  calcRootFluxes();
  checkCarbonLimitations();
  calcNResorptionFluxes();

  status |= checkFlux(fluxes.coarseRootCreation, -5.6,
                      "coarseRootCreation (negative fine roots)");
  status |= checkFlux(fluxes.fineRootCreation, -4.0,
                      "fineRootCreation (negative fine roots)");
  status |= checkFlux(fluxes.reductionNResorption, 0.156,
                      "reductionNResorption (negative fine roots)");

  // Case 3: negative coarse root
  // Expected:
  // coarseRootCreation = -32.0 * 0.1 = -3.2 (+1.6) = -1.6
  // fineRootCreation = -32.0 * 0.2 = -6.4 (-2.6) = -8.0
  // [envi.coarseRootC = 0.2*8 = 1.6; 1.6 moves to coarse]
  // npp < 0:
  //   leafOffNResorption += 1.6 / 100.0 = 0.016
  //   leafOffNResorption += 8.0 / 40.0  = 0.2
  //   total = 0.216
  initRootFluxesNegativeNPPState(-32.0, 0.2, 5.0);
  calcRootFluxes();
  checkCarbonLimitations();
  calcNResorptionFluxes();

  status |= checkFlux(fluxes.coarseRootCreation, -1.6,
                      "coarseRootCreation (negative fine roots)");
  status |= checkFlux(fluxes.fineRootCreation, -8.0,
                      "fineRootCreation (negative fine roots)");
  status |= checkFlux(fluxes.reductionNResorption, 0.216,
                      "reductionNResorption (negative fine roots)");

  return status;
}

int run(void) {
  int status = 0;

  setupTests();

  status |= testWoodAndLeafFluxesPositiveNPP();
  status |= testWoodAndLeafFluxesNegativeNPP();
  status |= testWoodAndLeafFluxesLeafDeficit();
  status |= testWoodAndLeafFluxesWithAccountingDelta();
  status |= testRootFluxesPositiveNPP();
  status |= testRootFluxesNegativeNPP();

  return status;
}

int main(void) {
  int status;

  logTest("Starting testFluxCalculations:run()\n");
  status = run();
  if (status) {
    logTest("FAILED testFluxCalculations with status %d\n", status);
    exit(status);
  }

  logTest("PASSED testFluxCalculations\n");
}

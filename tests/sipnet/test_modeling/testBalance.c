#include "utils/tUtils.h"
#include "sipnet/events.c"
#include "sipnet/sipnet.c"

#define TEST_EPS 1e-8

// For help with debugging failures
#define DEBUG_C 0
#define DEBUG_N 0
#define DO_OUTPUT 0

#if DO_OUTPUT
FILE *out;
#endif

// NOTE: this is much more of a regression test than a proper unit test, but so
// be it. This will hopefully be useful for debugging balance issues once
// complete.

void reset(void) { setupModel(); }

void setupTests(ModelParams **modelParamsPtr) {
  // Set up the context
  initContext();
  ctx.litterPool = 1;
  ctx.nitrogenCycle = 1;
  ctx.gdd = 0;

  initModel(modelParamsPtr, "balance.param", "balance.clim");
  initEvents(EVENT_IN_FILE, ctx.printHeader);
  reset();
#if DO_OUTPUT
  out = openFile("balance.out", "w");
  outputHeader(out);
#endif
}

void dumpState(void);

void step(void) {
  updateState();
#if DO_OUTPUT
  outputState(out, climate->year, climate->day, climate->time);
#endif
  dumpState();
}

int checkCarbon(void) {
  int status = 0;
  if (fabs(balanceTracker.deltaC) > TEST_EPS) {
    status = 1;
    logTest("carbon balance check delta %8.5f (Y %d D %d T %4.2f)\n",
            balanceTracker.deltaC, climate->year, climate->day, climate->time);
  }
  return status;
}

int checkNitrogen(void) {
  int status = 0;
  if (fabs(balanceTracker.deltaN) > TEST_EPS) {
    // ENABLE WHEN NITROGEN IS FINISHED AND LEAF ON/OFF IS HANDLED
    // status = 1;
    logTest("nitrogen balance check delta %8.5f (Y %d D %d T %4.2f)\n",
            balanceTracker.deltaN, climate->year, climate->day, climate->time);
  }
  return status;
}

int testBalanceSimple(void) {
  logTest("Starting testBalanceSimple()\n");
  int status = 0;

  // One-step test
  step();

  status |= checkCarbon();
  status |= checkNitrogen();

  return status;
}

int testBalanceLeaf(void) {
  logTest("Starting testBalanceLeaf()\n");
  int status = 0;

  // Run the whole climate file (5 days), which includes a leaf-on event and a
  // leaf-off event
  while (climate != NULL) {
    step();

    status |= checkCarbon();
    status |= checkNitrogen();

    climate = climate->nextClim;
  }

  return status;
}

int run(void) {
  int status = 0;

  ModelParams *modelParams;
  setupTests(&modelParams);

  status |= testBalanceSimple();
  reset();

  status |= testBalanceLeaf();

  return status;
}

int main(void) {
  int status;

  logTest("Starting testBalance:run()\n");
  status = run();
#if DO_OUTPUT
  fclose(out);
#endif

  if (status) {
    logTest("FAILED testBalance with status %d\n", status);
    exit(status);
  }

  logTest("PASSED testBalance\n");
}

void dumpState(void) {
#if DEBUG_C | DEBUG_N
  double len = climate->length;
#endif
#if DEBUG_C
  logTest("preC %10.6f postC %10.6f inC %10.6f outC %10.6f pDelta %10.6f "
          "sDelta %10.6f delta %10.6f\n",
          balanceTracker.preTotalC, balanceTracker.postTotalC,
          balanceTracker.inputsC, balanceTracker.outputsC,
          balanceTracker.postTotalC - balanceTracker.preTotalC,
          balanceTracker.inputsC - balanceTracker.outputsC,
          balanceTracker.deltaC);
  logTest("  C:        wood %10.6f leaf %10.6f fine %10.6f coarse %10.6f "
          "soil %10.6f litter %10.6f\n",
          envi.plantWoodC, envi.plantLeafC, envi.fineRootC, envi.coarseRootC,
          envi.soilC, envi.litterC);
  logTest("  creation: wood %10.6f leaf %10.6f fine %10.6f coarse %10.6f\n",
          fluxes.woodCreation * len, fluxes.leafCreation * len,
          fluxes.fineRootCreation * len, fluxes.coarseRootCreation * len);
  logTest("  loss:     wood %10.6f leaf %10.6f fine %10.6f coarse %10.6f\n",
          fluxes.woodLitter * len, fluxes.leafLitter * len,
          fluxes.fineRootLoss * len, fluxes.coarseRootLoss * len);
#endif
#if DEBUG_N
  logTest("preN %10.6f postN %10.6f inN %10.6f outN %10.6f pDelta %10.6f "
          "sDelta %10.6f delta %10.6f\n",
          balanceTracker.preTotalN, balanceTracker.postTotalN,
          balanceTracker.inputsN, balanceTracker.outputsN,
          balanceTracker.postTotalN - balanceTracker.preTotalN,
          balanceTracker.inputsN - balanceTracker.outputsN,
          balanceTracker.deltaN);
  logTest("  N:        wood %10.6f leaf %10.6f fine %10.6f coarse %10.6f "
          "soil %10.6f litter %10.6f min %10.6f\n",
          envi.plantWoodC / params.woodCN, envi.plantLeafC / params.leafCN,
          envi.fineRootC / params.fineRootCN, envi.coarseRootC / params.woodCN,
          envi.soilOrgN, envi.litterN, envi.minN);
  logTest("  creation: wood %10.6f leaf %10.6f fine %10.6f coarse %10.6f\n",
          fluxes.woodCreation * len / params.woodCN,
          fluxes.leafCreation * len / params.leafCN,
          fluxes.fineRootCreation * len / params.fineRootCN,
          fluxes.coarseRootCreation * len / params.woodCN);
  logTest("  loss:     wood %10.6f leaf %10.6f fine %10.6f coarse %10.6f\n",
          fluxes.woodLitter * len / params.woodCN,
          fluxes.leafLitter * len / params.leafCN,
          fluxes.fineRootLoss * len / params.fineRootCN,
          fluxes.coarseRootLoss * len / params.woodCN);
#endif
}

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/logging.h"
#include "utils/tUtils.h"

#include "sipnet/events.c"
#include "sipnet/sipnet.c"

#define TEST_EPS 1e-8
#define OUTPUT_FILE "wood_storage.out"
#define HARVEST_FILE "storage_harvest.in"

static ClimateNode testClimate;

static int checkClose(const char *label, double actual, double expected) {
  if (fabs(actual - expected) <= TEST_EPS) {
    return 0;
  }

  logTest("%s is %f, expected %f\n", label, actual, expected);
  return 1;
}

static void initStorageTestState(void) {
  initContext();
  updateIntContext("events", 1, CTX_TEST);

  testClimate = (ClimateNode){0};
  testClimate.year = 2026;
  testClimate.day = 120;
  testClimate.time = 12.0;
  testClimate.length = 1.0;
  testClimate.tair = 0.0;
  testClimate.tsoil = 5.0;
  climate = &testClimate;
  firstClimate = &testClimate;

  if (meanNPP == NULL) {
    meanNPP = newMeanTracker(0.0, MEAN_NPP_DAYS, MEAN_NPP_MAX_ENTRIES);
  } else {
    resetMeanTracker(meanNPP, 0.0);
  }

  params.baseVegResp = 1.0;
  params.vegRespQ10 = 1.0;
  params.woodTurnoverRate = 0.5;
  params.coarseRootTurnoverRate = 0.0;
  params.fineRootTurnoverRate = 0.0;
  params.coarseRootAllocation = 0.0;
  params.fineRootAllocation = 0.0;
  params.woodAllocation = 0.0;
  params.leafCN = 20.0;
  params.woodCN = 10.0;
  params.fineRootCN = 30.0;

  envi = (Envi){0};
  envi.soilC = 10.0;
  envi.soilWater = 5.0;

  initTrackers();
  resetFluxes();
  initBalanceTracker();
}

static int getOutputValue(const char *name, double *value) {
  FILE *out = fopen(OUTPUT_FILE, "w");
  if (out == NULL) {
    logTest("Unable to open %s for writing\n", OUTPUT_FILE);
    return 1;
  }

  outputHeader(out);
  outputState(out, testClimate.year, testClimate.day, testClimate.time);
  fclose(out);

  out = fopen(OUTPUT_FILE, "r");
  if (out == NULL) {
    logTest("Unable to open %s for reading\n", OUTPUT_FILE);
    return 1;
  }

  char header[4096];
  char values[4096];
  if (fgets(header, sizeof(header), out) == NULL ||
      fgets(values, sizeof(values), out) == NULL) {
    fclose(out);
    logTest("Unable to read output lines from %s\n", OUTPUT_FILE);
    return 1;
  }
  fclose(out);

  int targetIndex = -1;
  int index = 0;
  char *headerToken = strtok(header, " \t\n");
  while (headerToken != NULL) {
    if (strcmp(headerToken, name) == 0) {
      targetIndex = index;
      break;
    }
    index++;
    headerToken = strtok(NULL, " \t\n");
  }
  if (targetIndex < 0) {
    logTest("Unable to find output column %s\n", name);
    return 1;
  }

  index = 0;
  char *valueToken = strtok(values, " \t\n");
  while (valueToken != NULL) {
    if (index == targetIndex) {
      *value = atof(valueToken);
      return 0;
    }
    index++;
    valueToken = strtok(NULL, " \t\n");
  }

  logTest("Unable to find value for output column %s\n", name);
  return 1;
}

static int testOutputSemantics(void) {
  int status = 0;
  double reportedWood = 0.0;
  double storage = 0.0;

  logTest("Starting testOutputSemantics()\n");
  initStorageTestState();

  envi.plantWoodC = 100.0;
  envi.plantWoodCStorageDelta = 25.0;
  envi.plantLeafC = 5.0;

  status |= getOutputValue("plantWoodC", &reportedWood);
  status |= getOutputValue("nppStorage", &storage);
  status |= checkClose("reported plantWoodC", reportedWood, 125.0);
  status |= checkClose("nppStorage", storage, 25.0);

  return status;
}

static int testNegativeStorageDoesNotReduceWoodFluxes(void) {
  int status = 0;
  double folResp = 0.0;
  double woodResp = 0.0;

  logTest("Starting testNegativeStorageDoesNotReduceWoodFluxes()\n");
  initStorageTestState();

  envi.plantWoodC = 3.0;
  envi.plantWoodCStorageDelta = -5.0;

  vegResp(&folResp, &woodResp, 0.0);
  calcRootAndWoodFluxes();

  status |= checkClose("woodResp", woodResp, 3.0);
  status |= checkClose("woodLitter", fluxes.woodLitter, 1.5);
  status |= checkClose("storage-backed wood carbon",
                       getStorageBackedWoodCarbon(), 3.0);

  return status;
}

static int writeHarvestEventFile(void) {
  FILE *events = fopen(HARVEST_FILE, "w");
  if (events == NULL) {
    logTest("Unable to open %s for writing\n", HARVEST_FILE);
    return 1;
  }

  fprintf(events, "%d %d harv 1 0 0 0\n", testClimate.year, testClimate.day);
  fclose(events);
  return 0;
}

static void runHarvestEvent(void) {
  initEvents(HARVEST_FILE, "events.out", 0);
  setupEvents();
  processEvents();
  updateBalanceTrackerPreUpdate();
  updatePoolsForEvents();
  updateBalanceTrackerPostUpdate();
  ensureNonNegativeStocks();
  updateBalanceTrackerPostClamp();
  checkBalance();
  closeEventOutFile();
}

static int testHarvestRemovesPositiveStorage(void) {
  int status = 0;

  logTest("Starting testHarvestRemovesPositiveStorage()\n");
  initStorageTestState();
  status |= writeHarvestEventFile();

  envi.plantWoodC = 10.0;
  envi.plantWoodCStorageDelta = 5.0;
  envi.plantLeafC = 2.0;

  runHarvestEvent();

  status |= checkClose("plantWoodC", envi.plantWoodC, 0.0);
  status |=
      checkClose("plantWoodCStorageDelta", envi.plantWoodCStorageDelta, 0.0);
  status |= checkClose("eventWoodC", fluxes.eventWoodC, -10.0);
  status |=
      checkClose("eventWoodStorageDelta", fluxes.eventWoodStorageDelta, -5.0);
  status |= checkClose("eventOutputC", fluxes.eventOutputC, 17.0);
  status |= checkClose("deltaC", balanceTracker.deltaC, 0.0);

  return status;
}

static int testHarvestWithNegativeStorageLeavesDebtVisible(void) {
  int status = 0;

  logTest("Starting testHarvestWithNegativeStorageLeavesDebtVisible()\n");
  initStorageTestState();
  status |= writeHarvestEventFile();

  envi.plantWoodC = 10.0;
  envi.plantWoodCStorageDelta = -15.0;
  envi.plantLeafC = 2.0;

  runHarvestEvent();

  status |= checkClose("plantWoodC", envi.plantWoodC, 0.0);
  status |=
      checkClose("plantWoodCStorageDelta", envi.plantWoodCStorageDelta, -15.0);
  status |= checkClose("total plantWoodC", getPlantWoodCTotal(), -15.0);
  status |= checkClose("eventWoodC", fluxes.eventWoodC, -10.0);
  status |=
      checkClose("eventWoodStorageDelta", fluxes.eventWoodStorageDelta, 0.0);
  status |= checkClose("eventOutputC", fluxes.eventOutputC, 12.0);
  status |= checkClose("deltaC", balanceTracker.deltaC, 0.0);

  return status;
}

int main(void) {
  int status = 0;

  logTest("Starting testWoodStorage\n");

  status |= testOutputSemantics();
  status |= testNegativeStorageDoesNotReduceWoodFluxes();
  status |= testHarvestRemovesPositiveStorage();
  status |= testHarvestWithNegativeStorageLeavesDebtVisible();

  if (status) {
    logTest("FAILED testWoodStorage with status %d\n", status);
    exit(status);
  }

  logTest("PASSED testWoodStorage\n");
}

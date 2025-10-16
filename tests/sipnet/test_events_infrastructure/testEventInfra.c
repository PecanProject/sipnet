
#include <stdio.h>
#include <stdlib.h>

#include "utils/tUtils.h"
#include "common/logging.h"
#include "sipnet/events.c"

int checkEvent(EventNode *event, int year, int day, enum EventType type) {
  if (!event) {
    return 1;
  }
  int success = event->year == year && event->day == day && event->type == type;
  if (!success) {
    logTest("Error checking event\n");
    printEvent(event);
    return 1;
  }
  return 0;
}

int checkHarvestParams(HarvestParams *hParams, double fracRemAbove,
                       double fracRemBelow, double fracTransAbove,
                       double fracTransBelow) {
  int status =
      !(compareDoubles(hParams->fractionRemovedAbove, fracRemAbove) &&
        compareDoubles(hParams->fractionRemovedBelow, fracRemBelow) &&
        compareDoubles(hParams->fractionTransferredAbove, fracTransAbove) &&
        compareDoubles(hParams->fractionTransferredBelow, fracTransBelow));
  if (status) {
    logTest("checkHarvestParams failed\n");
  }
  return status;
}

int checkIrrigationParams(IrrigationParams *iParams, double amountAdded,
                          int method) {
  int status = !(compareDoubles(iParams->amountAdded, amountAdded) &&
                 iParams->method == method);
  if (status) {
    logTest("checkIrrigationParams failed\n");
  }
  return status;
}

int checkFertilizationParams(FertilizationParams *fParams, double orgN,
                             double orgC, double minN) {
  int status = !(compareDoubles(fParams->orgN, orgN) &&
                 compareDoubles(fParams->orgC, orgC) &&
                 compareDoubles(fParams->minN, minN));
  if (status) {
    logTest("checkFertilizationParams failed\n");
  }
  return status;
}

int checkPlantingParams(PlantingParams *pParams, double leafC, double woodC,
                        double fineRootC, double coarseRootC) {
  int status = !(compareDoubles(pParams->leafC, leafC) &&
                 compareDoubles(pParams->woodC, woodC) &&
                 compareDoubles(pParams->fineRootC, fineRootC) &&
                 compareDoubles(pParams->coarseRootC, coarseRootC));
  if (status) {
    logTest("checkPlantingParams failed\n");
  }
  return status;
}

int checkTillageParams(TillageParams *tParams, double tillageEffect) {
  int status = !(compareDoubles(tParams->tillageEffect, tillageEffect));
  if (status) {
    logTest("checkTillageParams failed\n");
  }
  return status;
}

int init(void) { return 0; }

int runTestEmpty(void) {
  int status = 0;
  // Should return empty for no file
  EventNode *output = readEventData("infra_events_no_file.in");
  status |= (output != NULL);
  output = readEventData("infra_events_empty_file.in");
  status |= (output != NULL);

  return status;
}

int runTestSimple(void) {
  // Simple test with one loc, one outEvent per type
  EventNode *output = readEventData("infra_events_simple.in");

  if (!output) {
    logTest("runTestSimple: events not read properly\n");
    return 1;
  }

  // check output is correct
  int status = 0;
  EventNode *outEvent = output;
  status |= checkEvent(outEvent, 2022, 40, IRRIGATION);
  status |=
      checkIrrigationParams((IrrigationParams *)outEvent->eventParams, 5, 0);
  outEvent = outEvent->nextEvent;
  status |= checkEvent(outEvent, 2022, 40, FERTILIZATION);
  status |= checkFertilizationParams(
      (FertilizationParams *)outEvent->eventParams, 15, 5, 10);
  outEvent = outEvent->nextEvent;
  status |= checkEvent(outEvent, 2022, 45, TILLAGE);
  status |= checkTillageParams((TillageParams *)outEvent->eventParams, 0.1);
  outEvent = outEvent->nextEvent;
  status |= checkEvent(outEvent, 2022, 46, PLANTING);
  status |=
      checkPlantingParams((PlantingParams *)outEvent->eventParams, 10, 5, 4, 3);
  outEvent = outEvent->nextEvent;
  status |= checkEvent(outEvent, 2022, 250, HARVEST);
  status |= checkHarvestParams((HarvestParams *)outEvent->eventParams, 0.4, 0.1,
                               0.2, 0.3);

  return status;
}

int run(void) {

  int testStatus = 0;
  int status;
  status = runTestEmpty();
  if (status) {
    logTest("runTestEmpty failed\n");
  }
  testStatus |= status;

  status = runTestSimple();
  if (status) {
    logTest("runTestSimple failed\n");
  }
  testStatus |= status;

  return testStatus;
}

void cleanup(void) {
  // Perform any cleanup as needed
  // None needed here, we can leave the copied file
}

int main(void) {
  int status;
  //  status = init();
  //  if (status) {
  //    printf("Test initialization failed with status %d\n", status);
  //    exit(status);
  //  } else {
  logTest("Test initialized\n");
  //  }

  logTest("Starting run()\n");
  status = run();
  if (status) {
    logTest("FAILED testEventInfra with status %d\n", status);
    exit(status);
  }

  logTest("PASSED testEventInfra\n");

  cleanup();
}

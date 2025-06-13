
#include <stdio.h>
#include <stdlib.h>

#include "modelStructures.h"  // NOLINT
#include "utils/tUtils.h"
#include "sipnet/events.c"

int checkEvent(EventNode *event, int year, int day, enum EventType type) {
  if (!event) {
    return 1;
  }
  int success = event->year == year && event->day == day && event->type == type;
  if (!success) {
    printf("Error checking event\n");
    printEvent(event);
    return 1;
  }
  return 0;
}

int checkHarvestParams(HarvestParams *params, double fracRemAbove,
                       double fracRemBelow, double fracTransAbove,
                       double fracTransBelow) {
  int status =
      !(compareDoubles(params->fractionRemovedAbove, fracRemAbove) &&
        compareDoubles(params->fractionRemovedBelow, fracRemBelow) &&
        compareDoubles(params->fractionTransferredAbove, fracTransAbove) &&
        compareDoubles(params->fractionTransferredBelow, fracTransBelow));
  if (status) {
    printf("checkHarvestParams failed\n");
  }
  return status;
}

int checkIrrigationParams(IrrigationParams *params, double amountAdded,
                          int method) {
  int status = !(compareDoubles(params->amountAdded, amountAdded) &&
                 params->method == method);
  if (status) {
    printf("checkIrrigationParams failed\n");
  }
  return status;
}

int checkFertilizationParams(FertilizationParams *params, double orgN,
                             double orgC, double minN) {
  int status = !(compareDoubles(params->orgN, orgN) &&
                 compareDoubles(params->orgC, orgC) &&
                 compareDoubles(params->minN, minN));
  if (status) {
    printf("checkFertilizationParams failed\n");
  }
  return status;
}

int checkPlantingParams(PlantingParams *params, double leafC, double woodC,
                        double fineRootC, double coarseRootC) {
  int status = !(compareDoubles(params->leafC, leafC) &&
                 compareDoubles(params->woodC, woodC) &&
                 compareDoubles(params->fineRootC, fineRootC) &&
                 compareDoubles(params->coarseRootC, coarseRootC));
  if (status) {
    printf("checkPlantingParams failed\n");
  }
  return status;
}

int checkTillageParams(TillageParams *params, double fracLitterTransferred,
                       double somDecompModifier, double litterDecompModifier) {
  int status =
      !(compareDoubles(params->fractionLitterTransferred,
                       fracLitterTransferred) &&
        compareDoubles(params->somDecompModifier, somDecompModifier) &&
        compareDoubles(params->litterDecompModifier, litterDecompModifier));
  if (status) {
    printf("checkTillageParams failed\n");
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
  // Simple test with one loc, one event per type
  EventNode *output = readEventData("infra_events_simple.in");

  if (!output) {
    printf("runTestSimple: events not read properly\n");
    return 1;
  }

  // check output is correct
  int status = 0;
  EventNode *event = output;
  status |= checkEvent(event, 2022, 40, IRRIGATION);
  status |= checkIrrigationParams((IrrigationParams *)event->eventParams, 5, 0);
  event = event->nextEvent;
  status |= checkEvent(event, 2022, 40, FERTILIZATION);
  status |= checkFertilizationParams((FertilizationParams *)event->eventParams,
                                     15, 5, 10);
  event = event->nextEvent;
  status |= checkEvent(event, 2022, 45, TILLAGE);
  status |=
      checkTillageParams((TillageParams *)event->eventParams, 0.1, 0.2, 0.3);
  event = event->nextEvent;
  status |= checkEvent(event, 2022, 46, PLANTING);
  status |=
      checkPlantingParams((PlantingParams *)event->eventParams, 10, 5, 4, 3);
  event = event->nextEvent;
  status |= checkEvent(event, 2022, 250, HARVEST);
  status |= checkHarvestParams((HarvestParams *)event->eventParams, 0.4, 0.1,
                               0.2, 0.3);

  return status;
}

int run(void) {

  int testStatus = 0;
  int status;
  status = runTestEmpty();
  if (status) {
    printf("runTestEmpty failed\n");
  }
  testStatus |= status;

  status = runTestSimple();
  if (status) {
    printf("runTestSimple failed\n");
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
  printf("Test initialized\n");
  //  }

  printf("Starting run()\n");
  status = run();
  if (status) {
    printf("FAILED testEventInfra with status %d\n", status);
    exit(status);
  }

  printf("PASSED testEventInfra\n");

  cleanup();
}

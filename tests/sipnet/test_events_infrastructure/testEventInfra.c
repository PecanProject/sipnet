
#include <stdio.h>
#include <stdlib.h>

#include "modelStructures.h"  // NOLINT
#include "utils/tUtils.h"
#include "sipnet/events.c"

int checkEvent(EventNode* event, int loc, int year, int day, enum EventType type) {
  int success =
    event->loc == loc &&
    event->year == year &&
    event->day == day &&
    event->type == type;
  if (!success) {
    printf("Error checking event\n");
    printEvent(event);
    return 1;
  }
  return 0;
}

int checkHarvestParams(
  HarvestParams *params, double fracRemAbove,
  double fracRemBelow,
  double fracTransAbove,
  double fracTransBelow
) {
  return !(
    compareDoubles(params->fractionRemovedAbove, fracRemAbove) &&
    compareDoubles(params->fractionRemovedBelow, fracRemBelow) &&
    compareDoubles(params->fractionTransferredAbove, fracTransAbove) &&
    compareDoubles(params->fractionTransferredBelow, fracTransBelow)
    );
}

int checkIrrigationParams(
  IrrigationParams *params,
  double amountAdded,
  int method
  ) {
  return !(
    compareDoubles(params->amountAdded, amountAdded) &&
    params->method == method
    );
}

int checkFertilizationParams(
  FertilizationParams *params,
  double orgN,
  double orgC,
  double minN
) {
  return !(
    compareDoubles(params->orgN, orgN) &&
    compareDoubles(params->orgC, orgC) &&
    compareDoubles(params->minN, minN)
    );
}

int checkPlantingParams(
  PlantingParams *params, int emergenceLag, double addedC, double addedN
  ) {
  return !(
    params->emergenceLag == emergenceLag &&
    compareDoubles(params->addedC, addedC) &&
    compareDoubles(params->addedN, addedN)
    );
}

int checkTillageParams(
  TillageParams *params,
  double fracLitterTransferred,
  double somDecompModifier,
  double litterDecompModifier
  ) {
  return !(
    compareDoubles(params->fractionLitterTransferred, fracLitterTransferred) &&
    compareDoubles(params->somDecompModifier, somDecompModifier) &&
    compareDoubles(params->litterDecompModifier, litterDecompModifier)
    );
}

int init() {
  // char * filename = "infra_events_simple.in";
  // if (access(filename, F_OK) == -1) {
  //   return 1;
  // }
  //
  // return copyFile(filename, "events.in");
  return 0;
}

int runTestSimple() {
  // Simple test with one loc, one event per type
  int numLocs = 1;
  EventNode** output = readEventData("infra_events_simple.in", numLocs);

  if (!output) {
    return 1;
  }

  // check output is correct
  int status = 0;
  EventNode *event = output[0];
  status |= checkEvent(event, 0, 2022, 40, IRRIGATION);
  status |= checkIrrigationParams((IrrigationParams*)event->eventParams, 5, 0);
  event = event->nextEvent;
  status |= checkEvent(event, 0, 2022, 40, FERTILIZATION);
  status |= checkFertilizationParams((FertilizationParams*)event->eventParams, 15, 5,10);
  event = event->nextEvent;
  status |= checkEvent(event, 0, 2022, 45, TILLAGE);
  status |= checkTillageParams((TillageParams*)event->eventParams, 0.1, 0.2, 0.3);
  event = event->nextEvent;
  status |= checkEvent(event, 0, 2022, 46, PLANTING);
  status |= checkPlantingParams((PlantingParams*)event->eventParams, 10, 10, 1);
  event = event->nextEvent;
  status |= checkEvent(event, 0, 2022, 250, HARVEST);
  status |= checkHarvestParams((HarvestParams*)event->eventParams, 0.4, 0.1, 0.2, 0.3);

  return status;
}

int runTestMulti() {
  // More complex test; multiple locations
  int numLocs = 1;
  EventNode** output = readEventData("infra_events_multi.in", numLocs);

  if (!output) {
    return 1;
  }

  // check output is correct
  int status = 0;
  int loc = 0;
  EventNode *event = output[loc];
  status |= checkEvent(event, loc, 2024, 60, TILLAGE);
  status |= checkTillageParams((TillageParams*)event->eventParams, 0.1, 0.2, 0.3);
  event = event->nextEvent;
  status |= checkEvent(event, loc, 2024, 65, PLANTING);
  status |= checkPlantingParams((PlantingParams*)event->eventParams, 15, 20, 5);
  event = event->nextEvent;
  status |= checkEvent(event, loc, 2024, 70, IRRIGATION);
  status |= checkIrrigationParams((IrrigationParams*)event->eventParams, 5, 1);
  event = event->nextEvent;
  status |= checkEvent(event, loc, 2024, 75, FERTILIZATION);
  status |= checkFertilizationParams((FertilizationParams*)event->eventParams, 15, 10,5);
  event = event->nextEvent;
  status |= checkEvent(event, loc, 2024, 250, HARVEST);
  status |= checkHarvestParams((HarvestParams*)event->eventParams, 0.1, 0.2, 0.3, 0.4);

  loc++;
  event = output[loc];
  status |= checkEvent(event, loc, 2024, 59, TILLAGE);
  status |= checkTillageParams((TillageParams*)event->eventParams, 0.2, 0.3, 0.1);
  event = event->nextEvent;
  status |= checkEvent(event, loc, 2024, 64, PLANTING);
  status |= checkPlantingParams((PlantingParams*)event->eventParams, 15, 20, 5);
  event = event->nextEvent;
  status |= checkEvent(event, loc, 2024, 69, IRRIGATION);
  status |= checkIrrigationParams((IrrigationParams*)event->eventParams, 5, 0);
  event = event->nextEvent;
  status |= checkEvent(event, loc, 2024, 74, FERTILIZATION);
  status |= checkFertilizationParams((FertilizationParams*)event->eventParams, 5, 15,10);
  event = event->nextEvent;
  status |= checkEvent(event, loc, 2024, 249, HARVEST);
  status |= checkHarvestParams((HarvestParams*)event->eventParams, 0.2, 0.3, 0.4, 0.1);

  loc++;
  event = output[loc];
  status |= checkEvent(event, loc, 2024, 58, TILLAGE);
  status |= checkTillageParams((TillageParams*)event->eventParams, 0.3, 0.1, 0.2);
  event = event->nextEvent;
  status |= checkEvent(event, loc, 2024, 63, PLANTING);
  status |= checkPlantingParams((PlantingParams*)event->eventParams, 15, 20, 5);
  event = event->nextEvent;
  status |= checkEvent(event, loc, 2024, 68, IRRIGATION);
  status |= checkIrrigationParams((IrrigationParams*)event->eventParams, 6, 1);
  event = event->nextEvent;
  status |= checkEvent(event, loc, 2024, 73, FERTILIZATION);
  status |= checkFertilizationParams((FertilizationParams*)event->eventParams, 5, 10,15);
  event = event->nextEvent;
  status |= checkEvent(event, loc, 2024, 248, HARVEST);
  status |= checkHarvestParams((HarvestParams*)event->eventParams, 0.3, 0.4, 0.1, 0.2);

  return status;
}

int run() {

  int status;

  status = runTestSimple();
  if (status) {
    return 1;
  }

  status = runTestMulti();
  if (status) {
    return 1;
  }

  return 0;
}

void cleanup() {
  // Perform any cleanup as needed
  // None needed here, we can leave the copied file
}

int main() {
  int status;
  status = init();
  if (status) {
    printf("Test initialization failed with status %d\n", status);
    exit(status);
  } else {
    printf("Test initialized\n");
  }

  printf("Starting run()\n");
  status = run();
  if (status) {
    printf("Test run failed with status %d\n", status);
    exit(status);
  }

  printf("testEventInfra PASSED\n");

  cleanup();
}
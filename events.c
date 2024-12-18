//
// events.c
//

#include "events.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "util.h"

void printEvent(EventNode* event);

EventNode* createEventNode(
    int loc, int year, int day, int eventType, char* eventParamsStr
) {
  EventNode *event = (EventNode*)malloc(sizeof(EventNode));
  event->loc = loc;
  event->year = year;
  event->day = day;
  event->type = eventType;

  switch (eventType) {
    case HARVEST:
      {
        double fracRA, fracRB, fracTA, fracTB;
        HarvestParams *params = (HarvestParams*)malloc(sizeof(HarvestParams));
        sscanf(eventParamsStr, "%lf %lf %lf %lf", &fracRA, &fracRB, &fracTA, &fracTB);
        params->fractionRemovedAbove = fracRA;
        params->fractionRemovedBelow = fracRB;
        params->fractionTransferredAbove = fracTA;
        params->fractionTransferredBelow = fracTB;
        event->eventParams = params;
      }
      break;
    case IRRIGATION:
      {
        double amountAdded;
        int location;
        IrrigationParams *params = (IrrigationParams*)malloc(sizeof(IrrigationParams));
        sscanf(eventParamsStr, "%lf %d", &amountAdded, &location);
        params->amountAdded = amountAdded;
        params->location = location;
        event->eventParams = params;
      }
      break;
    case FERTILIZATION:
      {
        // If/when we try two N pools, enable the additional nh4_no3_frac param (likely via
        // compiler switch)
        double orgN, orgC, minN;
        // double nh4_no3_frac;
        FertilizationParams *params = (FertilizationParams*)malloc(sizeof(FertilizationParams));
        sscanf(eventParamsStr, "%lf %lf %lf", &orgN, &orgC, &minN);
        // scanf(eventParamsStr, "%lf %lf %lf %lf", &org_N, &org_C, &min_N, &nh4_no3_frac);
        params->orgN = orgN;
        params->orgC = orgC;
        params->minN = minN;
        // params->nh4_no3_frac = nh4_nos_frac;
        event->eventParams = params;
      }
      break;
    case PLANTING:
      {
        int emergenceLag;
        double addedC, addedN;
        PlantingParams *params = (PlantingParams*)malloc(sizeof(PlantingParams));
        sscanf(eventParamsStr, "%d %lf %lf", &emergenceLag, &addedC, &addedN);
        params->emergenceLag = emergenceLag;
        params->addedC = addedC;
        params->addedN = addedN;
        event->eventParams = params;
      }
      break;
    case TILLAGE:
      {
        double fracLT, somDM, litterDM;
        TillageParams *params = (TillageParams*)malloc(sizeof(TillageParams));
        sscanf(eventParamsStr, "%lf %lf %lf", &fracLT, &somDM, &litterDM);
        params->fractionLitterTransferred = fracLT;
        params->somDecompModifier = somDM;
        params->litterDecompModifier = litterDM;
        event->eventParams = params;
      }
      break;
    default:
      // Unknown type, error and exit
      printf("Error reading from event file: unknown event type %d\n", eventType);
      exit(1);
  }

  event->nextEvent = NULL;
  return event;
}

enum EventType getEventType(char *eventTypeStr) {
  if (strcmp(eventTypeStr, "irrig") == 0) {
    return IRRIGATION;
  } else if (strcmp(eventTypeStr, "fert") == 0) {
    return FERTILIZATION;
  } else if (strcmp(eventTypeStr, "plant") == 0) {
    return PLANTING;
  } else if (strcmp(eventTypeStr, "till") == 0) {
    return TILLAGE;
  } else if (strcmp(eventTypeStr, "harv") == 0) {
    return HARVEST;
  }
  return UNKNOWN_EVENT;
}

EventNode** readEventData(char *eventFile, int numLocs) {
  int loc, year, day, eventType;
  int currLoc, currYear, currDay;
  int EVENT_LINE_SIZE = 1024;
  int numBytes;
  char *eventParamsStr;
  char eventTypeStr[20];
  char line[EVENT_LINE_SIZE];
  EventNode *curr, *next;

  printf("Begin reading event data from file %s\n", eventFile);

  EventNode** events = (EventNode**)calloc(sizeof(EventNode*), numLocs * sizeof(EventNode*));
  // status of the read
  FILE *in = openFile(eventFile, "r");

  if (fgets(line, EVENT_LINE_SIZE, in) == NULL) {
    printf("Error: no event data in %s\n", eventFile);
    exit(1);
  }
  sscanf(line, "%d %d %d %s %n", &loc, &year, &day, eventTypeStr, &numBytes);
  eventParamsStr = line + numBytes;

  eventType = getEventType(eventTypeStr);
  if (eventType == UNKNOWN_EVENT) {
    printf("Error: unknown event type %s\n", eventTypeStr);
    exit(1);
  }
  printf("Found event type %d (%s)\n", eventType, eventTypeStr);

  next = createEventNode(loc, year, day, eventType, eventParamsStr);
  events[loc] = next;
  currLoc = loc;
  currYear = year;
  currDay = day;

  printf("Found events:\n");
  printEvent(next);

  while (fgets(line, EVENT_LINE_SIZE, in) != NULL) {
    // We have another event
    curr = next;
    sscanf(line, "%d %d %d %s %n", &loc, &year, &day, eventTypeStr, &numBytes);
    eventParamsStr = line + numBytes;

    eventType = getEventType(eventTypeStr);
    if (eventType == UNKNOWN_EVENT) {
      printf("Error: unknown event type %s\n", eventTypeStr);
      exit(1);
    }

    // make sure location and time are non-decreasing
    if (loc < currLoc) {
      printf("Error reading event file: was reading location %d, trying to read location %d\n", currLoc, loc);
      printf("Event records for a given location should be contiguous, and locations should be in ascending order\n");
      exit(1);
    }
    if ((loc == currLoc) && ((year < currYear) || (day < currDay))) {
      printf("Error reading event file: for location %d, last event was at (%d, %d) ", currLoc, currYear, currDay);
      printf("next event is at (%d, %d)\n", year, day);
      printf("Event records for a given location should be in time-ascending order\n");
      exit(1);
    }

    next = createEventNode(loc, year, day, eventType, eventParamsStr);
    printEvent(next);
    if (currLoc == loc) {
      // Same location, add the new event to this location's list
      curr->nextEvent = next;
    }
    else {
      // New location, update location and start a new list
      currLoc = loc;
      events[currLoc] = next;
    }
  }

  fclose(in);
  return events;
}

void printEvent(EventNode *event) {
  if (event == NULL) {
    return;
  }
  int loc = event->loc;
  int year = event->year;
  int day = event->day;
  switch (event->type) {
    case IRRIGATION:
      printf("IRRIGATION at loc %d on %d %d, ", loc, year, day);
      IrrigationParams *const iParams = (IrrigationParams*)event->eventParams;
      printf("with params: amount added %4.2f\n", iParams->amountAdded);
      break;
    case FERTILIZATION:
      printf("FERTILIZATION at loc %d on %d %d, ", loc, year, day);
      FertilizationParams *const fParams = (FertilizationParams*)event->eventParams;
      printf("with params: org N %4.2f, org C %4.2f, min N %4.2f\n", fParams->orgN, fParams->orgC, fParams->minN);
      break;
    case PLANTING:
      printf("PLANTING at loc %d on %d %d, ", loc, year, day);
      PlantingParams *const pParams = (PlantingParams*)event->eventParams;
      printf("with params: emergence lag %d, added C %4.2f, added N %4.2f\n", pParams->emergenceLag, pParams->addedC, pParams->addedN);
      break;
    case TILLAGE:
      printf("TILLAGE at loc %d on %d %d, ", loc, year, day);
      TillageParams *const tParams = (TillageParams*)event->eventParams;
      printf("with params: frac litter transferred %4.2f, som decomp modifier %4.2f, litter decomp modifier %4.2f\n", tParams->fractionLitterTransferred, tParams->somDecompModifier, tParams->litterDecompModifier);
      break;
    case HARVEST:
      printf("HARVEST at loc %d on %d %d, ", loc, year, day);
      HarvestParams *const hParams = (HarvestParams*)event->eventParams;
      printf("with params: frac removed above %4.2f, frac removed below %4.2f, frac transferred above %4.2f, frac transferred below %4.2f\n", hParams->fractionRemovedAbove, hParams->fractionRemovedBelow, hParams->fractionTransferredAbove, hParams->fractionTransferredBelow);
      break;
    default:
      printf("Error printing event: unknown type %d\n", event->type);
  }
}

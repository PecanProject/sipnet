// clang-format off
/**
 * @file events.c
 * @brief Handles reading, parsing, and storing agronomic events for SIPNET simulations.
 *
 * The `events.in` file specifies agronomic events with the following columns:
 * | col | parameter   | description                                | units            |
 * |-----|-------------|--------------------------------------------|------------------|
 * | 1   | loc         | spatial location index                     |                  |
 * | 2   | year        | year of start of this timestep             |                  |
 * | 3   | day         | day of start of this timestep              | Day of year      |
 * | 4   | event_type  | type of event                              | (e.g., "irrig")  |
 * | 5...n| event_param| parameters specific to the event type      | (varies)         |
 *
 *
 * Event types include:
 * - "irrig": Parameters = [amount added (cm/d), type (0=canopy, 1=soil, 2=flood)]
 * - "fert": Parameters = [Org-N (g/m²), Org-C (g/ha), Min-N (g/m²), Min-N2 (g/m²)]
 * - "till": Parameters = [SOM decomposition modifier, litter decomposition modifier]
 * - "plant": Parameters = [leaf C (g/m²), woo C (g/m²), fine root C (g/m²), corase root C (g/m²)]
 * - "harv": Parameters = [fraction aboveground removed, fraction belowground removed, ...]
 * See test examples in `tests/sipnet/test_events_infrastructure/` and tests/sipnet/test_events_types/.
 */
// clang-format on

#include "events.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>  // for access()
#include "exitCodes.h"
#include "common/util.h"

void printEvent(EventNode *event);

EventNode *createEventNode(int loc, int year, int day, int eventType,
                           const char *eventParamsStr) {
  EventNode *event = (EventNode *)malloc(sizeof(EventNode));
  event->loc = loc;
  event->year = year;
  event->day = day;
  event->type = eventType;

  switch (eventType) {
    case HARVEST: {
      double fracRA, fracRB, fracTA, fracTB;
      HarvestParams *params = (HarvestParams *)malloc(sizeof(HarvestParams));
      int numRead = sscanf(eventParamsStr, "%lf %lf %lf %lf", &fracRA, &fracRB,
                           &fracTA, &fracTB);
      if (numRead != NUM_HARVEST_PARAMS) {
        printf("Error parsing Harvest params for loc %d year %d day %d\n", loc,
               year, day);
        exit(EXIT_CODE_INPUT_FILE_ERROR);
      }
      // Validate the params
      if ((fracRA + fracTA > 1) || (fracRB + fracTB > 1)) {
        printf("Invalid harvest event for loc %d year %d day %d; above and "
               "below must each add to 1 or less",
               loc, year, day);
        exit(EXIT_CODE_BAD_PARAMETER_VALUE);
      }
      params->fractionRemovedAbove = fracRA;
      params->fractionRemovedBelow = fracRB;
      params->fractionTransferredAbove = fracTA;
      params->fractionTransferredBelow = fracTB;
      event->eventParams = params;
    } break;
    case IRRIGATION: {
      double amountAdded;
      int method;
      IrrigationParams *params =
          (IrrigationParams *)malloc(sizeof(IrrigationParams));
      int numRead = sscanf(eventParamsStr, "%lf %d", &amountAdded, &method);
      if (numRead != NUM_IRRIGATION_PARAMS) {
        printf("Error parsing Irrigation params for loc %d year %d day %d\n",
               loc, year, day);
        exit(EXIT_CODE_INPUT_FILE_ERROR);
      }
      params->amountAdded = amountAdded;
      params->method = method;
      event->eventParams = params;
    } break;
    case FERTILIZATION: {
      // If/when we try two N pools, enable the additional nh4_no3_frac param
      // (likely via compiler switch)
      double orgN, orgC, minN;
      // double nh4_no3_frac;
      FertilizationParams *params =
          (FertilizationParams *)malloc(sizeof(FertilizationParams));
      int numRead = sscanf(eventParamsStr, "%lf %lf %lf", &orgN, &orgC, &minN);
      if (numRead != NUM_FERTILIZATION_PARAMS) {
        printf("Error parsing Fertilization params for loc %d year %d day %d\n",
               loc, year, day);
        exit(EXIT_CODE_INPUT_FILE_ERROR);
      }
      // scanf(eventParamsStr, "%lf %lf %lf %lf", &org_N, &org_C, &min_N,
      // &nh4_no3_frac);
      params->orgN = orgN;
      params->orgC = orgC;
      params->minN = minN;
      // params->nh4_no3_frac = nh4_nos_frac;
      event->eventParams = params;
    } break;
    case PLANTING: {
      double leafC, woodC, fineRootC, coarseRootC;
      PlantingParams *params = (PlantingParams *)malloc(sizeof(PlantingParams));
      int numRead = sscanf(eventParamsStr, "%lf %lf %lf %lf", &leafC, &woodC,
                           &fineRootC, &coarseRootC);
      if (numRead != NUM_PLANTING_PARAMS) {
        printf("Error parsing Planting params for loc %d year %d day %d\n", loc,
               year, day);
        exit(EXIT_CODE_INPUT_FILE_ERROR);
      }
      params->leafC = leafC;
      params->woodC = woodC;
      params->fineRootC = fineRootC;
      params->coarseRootC = coarseRootC;
      event->eventParams = params;
    } break;
    case TILLAGE: {
      double fracLT, somDM, litterDM;
      TillageParams *params = (TillageParams *)malloc(sizeof(TillageParams));
      int numRead =
          sscanf(eventParamsStr, "%lf %lf %lf", &fracLT, &somDM, &litterDM);
      if (numRead != NUM_TILLAGE_PARAMS) {
        printf("Error parsing Tillage params for loc %d year %d day %d\n", loc,
               year, day);
        exit(EXIT_CODE_INPUT_FILE_ERROR);
      }
      params->fractionLitterTransferred = fracLT;
      params->somDecompModifier = somDM;
      params->litterDecompModifier = litterDM;
      event->eventParams = params;
    } break;
    default:
      // Unknown type, error and exit
      printf("Error reading event file: unknown event type %d\n", eventType);
      exit(1);
  }

  event->nextEvent = NULL;
  return event;
}

event_type_t getEventType(const char *eventTypeStr) {
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

EventNode **readEventData(char *eventFile, int numLocs) {
  int loc, year, day, eventType;
  int currLoc, currYear, currDay;
  int EVENT_LINE_SIZE = 1024;
  int numBytes;
  char *eventParamsStr;
  char eventTypeStr[20];
  char line[EVENT_LINE_SIZE];
  EventNode *curr, *next;

  EventNode **events =
      (EventNode **)calloc(sizeof(EventNode *), numLocs * sizeof(EventNode *));

  // Check for a non-empty file
  if (access(eventFile, F_OK) != 0) {
    // no file found, which is fine; we're done, a vector of NULL is what we
    // want for events
    printf("No event file found, assuming no events\n");
    return events;
  }

  printf("Begin reading event data from file %s\n", eventFile);

  FILE *in = openFile(eventFile, "r");

  if (fgets(line, EVENT_LINE_SIZE, in) == NULL) {
    // Again, this is fine - just return the empty events array
    return events;
  }

  int numRead = sscanf(line, "%d %d %d %s %n", &loc, &year, &day, eventTypeStr,
                       &numBytes);
  if (numRead != NUM_EVENT_CORE_PARAMS) {
    printf("Error reading event file: bad data on first line\n");
    exit(EXIT_CODE_INPUT_FILE_ERROR);
  }
  eventParamsStr = line + numBytes;

  eventType = getEventType(eventTypeStr);
  if (eventType == UNKNOWN_EVENT) {
    printf("Error reading event file: unknown event type %s\n", eventTypeStr);
    exit(EXIT_CODE_UNKNOWN_EVENT_TYPE_OR_PARAM);
  }

  next = createEventNode(loc, year, day, eventType, eventParamsStr);
  events[loc] = next;
  currLoc = loc;
  currYear = year;
  currDay = day;

  while (fgets(line, EVENT_LINE_SIZE, in) != NULL) {
    // We have another event
    curr = next;
    numRead = sscanf(line, "%d %d %d %s %n", &loc, &year, &day, eventTypeStr,
                     &numBytes);
    if (numRead != 4) {
      printf("Error reading event file: bad data on first line\n");
      exit(EXIT_CODE_INPUT_FILE_ERROR);
    }
    eventParamsStr = line + numBytes;

    eventType = getEventType(eventTypeStr);
    if (eventType == UNKNOWN_EVENT) {
      printf("Error reading event file: unknown event type %s\n", eventTypeStr);
      exit(EXIT_CODE_UNKNOWN_EVENT_TYPE_OR_PARAM);
    }

    // make sure location and time are non-decreasing
    if (loc < currLoc) {
      printf("Error reading event file: was reading location %d, trying to "
             "read location %d\n",
             currLoc, loc);
      printf("Event records for a given location should be contiguous, and "
             "locations should be in ascending order\n");
      exit(EXIT_CODE_INPUT_FILE_ERROR);
    }
    if ((loc == currLoc) && ((year < currYear) || (day < currDay))) {
      printf("Error reading event file: for location %d, last event was at "
             "(%d, %d) ",
             currLoc, currYear, currDay);
      printf("next event is at (%d, %d)\n", year, day);
      printf("Event records for a given location should be in time-ascending "
             "order\n");
      exit(EXIT_CODE_INPUT_FILE_ERROR);
    }

    next = createEventNode(loc, year, day, eventType, eventParamsStr);
    if (currLoc == loc) {
      // Same location, add the new event to this location's list
      curr->nextEvent = next;
    } else {
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
      IrrigationParams *const iParams = (IrrigationParams *)event->eventParams;
      printf("with params: amount added %4.2f\n", iParams->amountAdded);
      break;
    case FERTILIZATION:
      printf("FERTILIZATION at loc %d on %d %d, ", loc, year, day);
      FertilizationParams *const fParams =
          (FertilizationParams *)event->eventParams;
      printf("with params: org N %4.2f, org C %4.2f, min N %4.2f\n",
             fParams->orgN, fParams->orgC, fParams->minN);
      break;
    case PLANTING:
      printf("PLANTING at loc %d on %d %d, ", loc, year, day);
      PlantingParams *const pParams = (PlantingParams *)event->eventParams;
      printf("with params: leaf C %4.2f, wood C %4.2f, fine root C %4.2f, "
             "coarse root C %4.2f\n",
             pParams->leafC, pParams->woodC, pParams->fineRootC,
             pParams->coarseRootC);
      break;
    case TILLAGE:
      printf("TILLAGE at loc %d on %d %d, ", loc, year, day);
      TillageParams *const tParams = (TillageParams *)event->eventParams;
      printf("with params: frac litter transferred %4.2f, som decomp modifier "
             "%4.2f, litter decomp modifier %4.2f\n",
             tParams->fractionLitterTransferred, tParams->somDecompModifier,
             tParams->litterDecompModifier);
      break;
    case HARVEST:
      printf("HARVEST at loc %d on %d %d, ", loc, year, day);
      HarvestParams *const hParams = (HarvestParams *)event->eventParams;
      printf("with params: frac removed above %4.2f, frac removed below %4.2f, "
             "frac transferred above %4.2f, frac transferred below %4.2f\n",
             hParams->fractionRemovedAbove, hParams->fractionRemovedBelow,
             hParams->fractionTransferredAbove,
             hParams->fractionTransferredBelow);
      break;
    default:
      printf("Error printing event: unknown type %d\n", event->type);
  }
}

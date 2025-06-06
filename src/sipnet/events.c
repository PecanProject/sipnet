// clang-format off
/**
 * @file events.c
 * @brief Handles reading, parsing, and storing agronomic events for SIPNET simulations.
 *
 * The `events.in` file specifies agronomic events with the following columns:
 * | col  | parameter   | description                                | units            |
 * |------|-------------|--------------------------------------------|------------------|
 * | 1    | year        | year of start of this timestep             |                  |
 * | 2    | day         | day of start of this timestep              | Day of year      |
 * | 3    | event_type  | type of event                              | (e.g., "irrig")  |
 * | 4...n| event_param | parameters specific to the event type      | (varies)         |
 *
 *
 * Event types include:
 * - "irrig": Parameters = [amount added (cm/d), type (0=canopy, 1=soil, 2=flood)]
 * - "fert": Parameters = [Org-N (g/m²), Org-C (g/ha), Min-N (g/m²), Min-N2 (g/m²)]
 * - "till": Parameters = [SOM decomposition modifier, litter decomposition modifier]
 * - "plant": Parameters = [leaf C (g/m²), wood C (g/m²), fine root C (g/m²), corase root C (g/m²)]
 * - "harv": Parameters = [fraction aboveground removed, fraction belowground removed, ...]
 * See test examples in `tests/sipnet/test_events_infrastructure/` and tests/sipnet/test_events_types/.
 */
// clang-format on

#include "events.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>  // for access()
#include "common/exitCodes.h"
#include "common/util.h"

void printEvent(EventNode *event);

EventNode *createEventNode(int year, int day, int eventType,
                           const char *eventParamsStr) {
  EventNode *event = (EventNode *)malloc(sizeof(EventNode));
  event->year = year;
  event->day = day;
  event->type = eventType;

  switch (eventType) {
    case HARVEST: {
      double fracRA, fracRB, fracTA, fracTB;
      HarvestParams *params = (HarvestParams *)malloc(sizeof(HarvestParams));
      int numRead =
          sscanf(eventParamsStr,  // NOLINT
                 "%lf %lf %lf %lf", &fracRA, &fracRB, &fracTA, &fracTB);
      if (numRead != NUM_HARVEST_PARAMS) {
        printf("Error parsing Harvest params for year %d day %d\n", year, day);
        exit(EXIT_CODE_INPUT_FILE_ERROR);
      }
      // Validate the params
      if ((fracRA + fracTA > 1) || (fracRB + fracTB > 1)) {
        printf("Invalid harvest event for year %d day %d; above and below must "
               "each add to 1 or less",
               year, day);
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
      int numRead = sscanf(eventParamsStr,  // NOLINT
                           "%lf %d", &amountAdded, &method);
      if (numRead != NUM_IRRIGATION_PARAMS) {
        printf("Error parsing Irrigation params for year %d day %d\n", year,
               day);
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
      int numRead = sscanf(eventParamsStr,  // NOLINT
                           "%lf %lf %lf", &orgN, &orgC, &minN);
      if (numRead != NUM_FERTILIZATION_PARAMS) {
        printf("Error parsing Fertilization params for year %d day %d\n", year,
               day);
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
      int numRead =
          sscanf(eventParamsStr,  // NOLINT
                 "%lf %lf %lf %lf", &leafC, &woodC, &fineRootC, &coarseRootC);
      if (numRead != NUM_PLANTING_PARAMS) {
        printf("Error parsing Planting params for year %d day %d\n", year, day);
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
      int numRead = sscanf(eventParamsStr,  // NOLINT
                           "%lf %lf %lf", &fracLT, &somDM, &litterDM);
      if (numRead != NUM_TILLAGE_PARAMS) {
        printf("Error parsing Tillage params for year %d day %d\n", year, day);
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

EventNode *readEventData(char *eventFile) {
  int year, day, eventType;
  int currYear, currDay;
  int EVENT_LINE_SIZE = 1024;
  int numBytes;
  char *eventParamsStr;
  char eventTypeStr[20];
  char line[EVENT_LINE_SIZE];
  EventNode *curr, *next;
  EventNode *events = NULL;

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

  int numRead = sscanf(line,  // NOLINT
                       "%d %d %s %n", &year, &day, eventTypeStr, &numBytes);
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

  events = createEventNode(year, day, eventType, eventParamsStr);
  next = events;
  currYear = year;
  currDay = day;

  while (fgets(line, EVENT_LINE_SIZE, in) != NULL) {
    // We have another event
    curr = next;
    numRead = sscanf(line, "%d %d %s %n",  // NOLINT
                     &year, &day, eventTypeStr, &numBytes);
    if (numRead != NUM_EVENT_CORE_PARAMS) {
      printf(
          "Error reading event file: bad data on line after year %d day %d\n",
          currYear, currDay);
      exit(EXIT_CODE_INPUT_FILE_ERROR);
    }
    eventParamsStr = line + numBytes;

    eventType = getEventType(eventTypeStr);
    if (eventType == UNKNOWN_EVENT) {
      printf("Error reading event file: unknown event type %s\n", eventTypeStr);
      exit(EXIT_CODE_UNKNOWN_EVENT_TYPE_OR_PARAM);
    }

    if ((year < currYear) || ((year == currYear) && (day < currDay))) {
      printf("Error reading event file: last event was at (%d, %d) ", currYear,
             currDay);
      printf("next event is at (%d, %d)\n", year, day);
      printf("Event records must be in time-ascending order\n");
      exit(EXIT_CODE_INPUT_FILE_ERROR);
    }

    next = createEventNode(year, day, eventType, eventParamsStr);
    curr->nextEvent = next;
    currYear = year;
    currDay = day;
  }

  fclose(in);
  return events;
}

void printEvent(EventNode *event) {
  if (event == NULL) {
    return;
  }
  int year = event->year;
  int day = event->day;
  switch (event->type) {
    case IRRIGATION:
      printf("IRRIGATION on %d %d, ", year, day);
      IrrigationParams *const iParams = (IrrigationParams *)event->eventParams;
      printf("with params: amount added %4.2f\n", iParams->amountAdded);
      break;
    case FERTILIZATION:
      printf("FERTILIZATION on %d %d, ", year, day);
      FertilizationParams *const fParams =
          (FertilizationParams *)event->eventParams;
      printf("with params: org N %4.2f, org C %4.2f, min N %4.2f\n",
             fParams->orgN, fParams->orgC, fParams->minN);
      break;
    case PLANTING:
      printf("PLANTING on %d %d, ", year, day);
      PlantingParams *const pParams = (PlantingParams *)event->eventParams;
      printf("with params: leaf C %4.2f, wood C %4.2f, fine root C %4.2f, "
             "coarse root C %4.2f\n",
             pParams->leafC, pParams->woodC, pParams->fineRootC,
             pParams->coarseRootC);
      break;
    case TILLAGE:
      printf("TILLAGE at on %d %d, ", year, day);
      TillageParams *const tParams = (TillageParams *)event->eventParams;
      printf("with params: frac litter transferred %4.2f, som decomp modifier "
             "%4.2f, litter decomp modifier %4.2f\n",
             tParams->fractionLitterTransferred, tParams->somDecompModifier,
             tParams->litterDecompModifier);
      break;
    case HARVEST:
      printf("HARVEST on %d %d, ", year, day);
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

const char *eventTypeToString(event_type_t type) {
  switch (type) {
    case IRRIGATION:
      return "irrig";
    case PLANTING:
      return "plant";
    case HARVEST:
      return "harv";
    case FERTILIZATION:
      return "fert";
    case TILLAGE:
      return "till";
    default:
      printf("ERROR: unknown event type in eventTypeToString (%d)", type);
      exit(EXIT_CODE_UNKNOWN_EVENT_TYPE_OR_PARAM);
  }
}

FILE *openEventOutFile(int printHeader) {
  FILE *eventFile = openFile(EVENT_OUT_FILE, "w");
  if (printHeader) {
    // Use format string analogous to the one in writeEventOut for
    // better alignment (won't be perfect, but definitely better)
    fprintf(eventFile, "%4s  %3s  %-7s  %s", "year", "day", "type",
            "param_name=delta[,param_name=delta,...]\n");
  }
  return eventFile;
}

void writeEventOut(FILE *out, EventNode *event, int numParams, ...) {
  va_list args;
  int ind = 0;

  // Spec:
  // year day event_type <param name=delta>[,<param name>=<delta>,...]

  // Standard prefix for all
  fprintf(out, "%4d  %3d  %-7s  ", event->year, event->day,
          eventTypeToString(event->type));
  // Variable output per event type
  va_start(args, numParams);
  for (ind = 0; ind < numParams - 1; ind++) {
    fprintf(out, "%s=%-.2f,", va_arg(args, char *), va_arg(args, double));
  }
  fprintf(out, "%s=%-.2f\n", va_arg(args, char *), va_arg(args, double));
  va_end(args);
}

void closeEventOutFile(FILE *file) { fclose(file); }

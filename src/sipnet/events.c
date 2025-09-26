// clang-format off
/**
 * @file events.c
 * @brief Handles reading, parsing, and storing agronomic events for SIPNET simulations.
 *
 * The `events.in` file specifies agronomic events. See the input documentation
 * (currently parameters.md) for information on the format of that file. Also,
 * see test examples in `tests/sipnet/test_events_infrastructure/` and
 * tests/sipnet/test_events_types/.
 */
// clang-format on

#include "events.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>  // for access()

#include "common/exitCodes.h"
#include "common/logging.h"
#include "common/util.h"

#include "state.h"

// Global event variables - definition
EventNode *gEvents = NULL;
EventNode *gEvent = NULL;

// events.out handle, only needed here
static FILE *eventOutFile = NULL;

EventNode *createEventNode(int year, int day, int eventType,
                           const char *eventParamsStr) {
  EventNode *newEvent = (EventNode *)malloc(sizeof(EventNode));
  newEvent->year = year;
  newEvent->day = day;
  newEvent->type = eventType;

  switch (eventType) {
    case HARVEST: {
      double fracRA, fracRB, fracTA, fracTB;
      HarvestParams *hParams = (HarvestParams *)malloc(sizeof(HarvestParams));
      int numRead =
          sscanf(eventParamsStr,  // NOLINT
                 "%lf %lf %lf %lf", &fracRA, &fracRB, &fracTA, &fracTB);
      if (numRead != NUM_HARVEST_PARAMS) {
        logError("parsing Harvest params for year %d day %d\n", year, day);
        exit(EXIT_CODE_INPUT_FILE_ERROR);
      }
      // Validate the params
      if ((fracRA + fracTA > 1) || (fracRB + fracTB > 1)) {
        logError("nvalid harvest newEvent for year %d day %d; above and below "
                 "must each add to 1 or less",
                 year, day);
        exit(EXIT_CODE_BAD_PARAMETER_VALUE);
      }
      hParams->fractionRemovedAbove = fracRA;
      hParams->fractionRemovedBelow = fracRB;
      hParams->fractionTransferredAbove = fracTA;
      hParams->fractionTransferredBelow = fracTB;
      newEvent->eventParams = hParams;
    } break;
    case IRRIGATION: {
      double amountAdded;
      int method;
      IrrigationParams *iParams =
          (IrrigationParams *)malloc(sizeof(IrrigationParams));
      int numRead = sscanf(eventParamsStr,  // NOLINT
                           "%lf %d", &amountAdded, &method);
      if (numRead != NUM_IRRIGATION_PARAMS) {
        logError("parsing Irrigation params for year %d day %d\n", year, day);
        exit(EXIT_CODE_INPUT_FILE_ERROR);
      }
      iParams->amountAdded = amountAdded;
      iParams->method = method;
      newEvent->eventParams = iParams;
    } break;
    case FERTILIZATION: {
      // If/when we try two N pools, enable the additional nh4_no3_frac param
      // (likely via compiler switch)
      double orgN, orgC, minN;
      // double nh4_no3_frac;
      FertilizationParams *fParams =
          (FertilizationParams *)malloc(sizeof(FertilizationParams));
      int numRead = sscanf(eventParamsStr,  // NOLINT
                           "%lf %lf %lf", &orgN, &orgC, &minN);
      if (numRead != NUM_FERTILIZATION_PARAMS) {
        logError("parsing Fertilization params for year %d day %d\n", year,
                 day);
        exit(EXIT_CODE_INPUT_FILE_ERROR);
      }
      // scanf(eventParamsStr, "%lf %lf %lf %lf", &org_N, &org_C, &min_N,
      // &nh4_no3_frac);
      fParams->orgN = orgN;
      fParams->orgC = orgC;
      fParams->minN = minN;
      // params->nh4_no3_frac = nh4_nos_frac;
      newEvent->eventParams = fParams;
    } break;
    case PLANTING: {
      double leafC, woodC, fineRootC, coarseRootC;
      PlantingParams *pParams =
          (PlantingParams *)malloc(sizeof(PlantingParams));
      int numRead =
          sscanf(eventParamsStr,  // NOLINT
                 "%lf %lf %lf %lf", &leafC, &woodC, &fineRootC, &coarseRootC);
      if (numRead != NUM_PLANTING_PARAMS) {
        logError("parsing Planting params for year %d day %d\n", year, day);
        exit(EXIT_CODE_INPUT_FILE_ERROR);
      }
      pParams->leafC = leafC;
      pParams->woodC = woodC;
      pParams->fineRootC = fineRootC;
      pParams->coarseRootC = coarseRootC;
      newEvent->eventParams = pParams;
    } break;
    case TILLAGE: {
      double tillEffect;
      TillageParams *tParams = (TillageParams *)malloc(sizeof(TillageParams));
      int numRead = sscanf(eventParamsStr,  // NOLINT
                           "%lf", &tillEffect);
      if (numRead != NUM_TILLAGE_PARAMS) {
        logError("parsing Tillage params for year %d day %d\n", year, day);
        exit(EXIT_CODE_INPUT_FILE_ERROR);
      }
      tParams->tillageEffect = tillEffect;
      newEvent->eventParams = tParams;
    } break;
    default:
      // Unknown type, error and exit
      logError("reading newEvent file: unknown newEvent type %d\n", eventType);
      exit(1);
  }

  newEvent->nextEvent = NULL;
  return newEvent;
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
      logError("unknown event type in eventTypeToString (%d)", type);
      exit(EXIT_CODE_UNKNOWN_EVENT_TYPE_OR_PARAM);
  }
}

event_type_t eventStringToType(const char *eventTypeStr) {
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
  EventNode *newEvents = NULL;

  // Check for a non-empty file
  if (access(eventFile, F_OK) != 0) {
    // no file found, which is fine; we're done, a vector of NULL is what we
    // want for newEvents
    logInfo("No event file found, assuming no newEvents\n");
    return newEvents;
  }

  logInfo("Begin reading event data from file %s\n", eventFile);

  FILE *in = openFile(eventFile, "r");

  if (fgets(line, EVENT_LINE_SIZE, in) == NULL) {
    // Again, this is fine - just return the empty newEvents array
    return newEvents;
  }

  int numRead = sscanf(line,  // NOLINT
                       "%d %d %s %n", &year, &day, eventTypeStr, &numBytes);
  if (numRead != NUM_EVENT_CORE_PARAMS) {
    logError("reading event file: bad data on first line\n");
    exit(EXIT_CODE_INPUT_FILE_ERROR);
  }
  eventParamsStr = line + numBytes;

  eventType = eventStringToType(eventTypeStr);
  if (eventType == UNKNOWN_EVENT) {
    logError("reading event file: unknown event type %s\n", eventTypeStr);
    exit(EXIT_CODE_UNKNOWN_EVENT_TYPE_OR_PARAM);
  }

  newEvents = createEventNode(year, day, eventType, eventParamsStr);
  next = newEvents;
  currYear = year;
  currDay = day;

  while (fgets(line, EVENT_LINE_SIZE, in) != NULL) {
    // We have another event
    curr = next;
    numRead = sscanf(line, "%d %d %s %n",  // NOLINT
                     &year, &day, eventTypeStr, &numBytes);
    if (numRead != NUM_EVENT_CORE_PARAMS) {
      logError("reading event file: bad data on line after year %d day %d\n",
               currYear, currDay);
      exit(EXIT_CODE_INPUT_FILE_ERROR);
    }
    eventParamsStr = line + numBytes;

    eventType = eventStringToType(eventTypeStr);
    if (eventType == UNKNOWN_EVENT) {
      logError("reading event file: unknown event type %s\n", eventTypeStr);
      exit(EXIT_CODE_UNKNOWN_EVENT_TYPE_OR_PARAM);
    }

    if ((year < currYear) || ((year == currYear) && (day < currDay))) {
      // clang-format off
      logError("reading event file: last event was at (%d, %d), next event is "
               "at (%d, %d)\n", currDay, year, day);
      logError("event records must be in time-ascending order\n", currYear);
      // clang-format on
      exit(EXIT_CODE_INPUT_FILE_ERROR);
    }

    next = createEventNode(year, day, eventType, eventParamsStr);
    curr->nextEvent = next;
    currYear = year;
    currDay = day;
  }

  fclose(in);
  return newEvents;
}

void openEventOutFile(int printHeader) {
  eventOutFile = openFile(EVENT_OUT_FILE, "w");
  if (printHeader) {
    // Use format string analogous to the one in writeEventOut for
    // better alignment (won't be perfect, but definitely better)
    fprintf(eventOutFile, "%4s  %3s  %-7s  %s", "year", "day", "type",
            "param_name=delta[,param_name=delta,...]\n");
  }
}

void writeEventOut(EventNode *oneEvent, int numParams, ...) {
  va_list args;
  int ind = 0;

  // Spec:
  // year day event_type <param name=delta>[,<param name>=<delta>,...]

  // Standard prefix for all
  fprintf(eventOutFile, "%4d  %3d  %-7s  ", oneEvent->year, oneEvent->day,
          eventTypeToString(oneEvent->type));
  // Variable output per oneEvent type
  va_start(args, numParams);
  for (ind = 0; ind < numParams - 1; ind++) {
    fprintf(eventOutFile, "%s=%-.2f,", va_arg(args, char *),
            va_arg(args, double));
  }
  fprintf(eventOutFile, "%s=%-.2f\n", va_arg(args, char *),
          va_arg(args, double));
  va_end(args);
}

void closeEventOutFile() {
  if (eventOutFile) {
    fclose(eventOutFile);
  }
}

void initEvents(char *eventFile, int printHeader) {
  if (ctx.events) {
    gEvents = readEventData(eventFile);
    openEventOutFile(printHeader);
  }
}

void setupEvents() { gEvent = gEvents; }

void resetEventFluxes(void) {
  fluxes.eventLeafC = 0.0;
  fluxes.eventWoodC = 0.0;
  fluxes.eventFineRootC = 0.0;
  fluxes.eventCoarseRootC = 0.0;
  fluxes.eventEvap = 0.0;
  fluxes.eventSoilWater = 0.0;
  fluxes.eventLitterC = 0.0;
}

void processEvents(void) {
  // This should be in events.h/c, but with all the global state defined in
  // this file, let's leave it here for now. Maybe someday we will factor that
  // out.

  // Set all event fluxes to zero, as these have no memory from one step to
  // the next
  resetEventFluxes();

  // If event starts off NULL, this function will just fall through, as it
  // should.
  const int climYear = climate->year;
  const int climDay = climate->day;
  const double climLen = climate->length;

  // As this is used as a divisor in many places, let's make sure it's >0
  if (climLen <= 0) {
    logError("climate length (%f) on year %d day %d is non-positive; please "
             "fix and re-run",
             climLen, climYear, climDay);
    exit(EXIT_CODE_BAD_PARAMETER_VALUE);
  }

  // The events file has been tested on read, so we know this event list
  // should be in chrono order. However, we need to check to make sure the
  // current event is not in the past, as that would indicate an event that
  // did not have a corresponding climate file record.
  while (gEvent != NULL && gEvent->year <= climYear && gEvent->day <= climDay) {
    if (gEvent->year < climYear || gEvent->day < climDay) {
      logError("Agronomic event found for year: %d day: %d that does not "
               "have a corresponding record in the climate file\n",
               gEvent->year, gEvent->day);
      exit(EXIT_CODE_INPUT_FILE_ERROR);
    }
    switch (gEvent->type) {
      case IRRIGATION: {
        const IrrigationParams *irrParams = gEvent->eventParams;
        const double amount = irrParams->amountAdded;
        double soilAmount, evapAmount;
        if (irrParams->method == CANOPY) {
          // Part of the irrigation evaporates, and the rest makes it to the
          // soil. Evaporated fraction:
          evapAmount = params.immedEvapFrac * amount;
          // Soil fraction:
          soilAmount = amount - evapAmount;
        } else if (irrParams->method == SOIL) {
          // All goes to the soil
          evapAmount = 0.0;
          soilAmount = amount;
        } else {
          logError("Unknown irrigation method type: %d\n", irrParams->method);
          exit(EXIT_CODE_UNKNOWN_EVENT_TYPE_OR_PARAM);
        }
        fluxes.eventEvap += evapAmount / climLen;
        fluxes.eventSoilWater += soilAmount / climLen;
        writeEventOut(gEvent, 2, "fluxes.eventSoilWater", soilAmount / climLen,
                      "fluxes.eventEvap", evapAmount / climLen);
      } break;
      case PLANTING: {
        const PlantingParams *plantParams = gEvent->eventParams;
        const double leafC = plantParams->leafC;
        const double woodC = plantParams->woodC;
        const double fineRootC = plantParams->fineRootC;
        const double coarseRootC = plantParams->coarseRootC;

        // Update the fluxes
        fluxes.eventLeafC += leafC / climLen;
        fluxes.eventWoodC += woodC / climLen;
        fluxes.eventFineRootC += fineRootC / climLen;
        fluxes.eventCoarseRootC += coarseRootC / climLen;

        // FUTURE: allocate to N pools

        writeEventOut(gEvent, 4, "fluxes.eventLeafC", leafC / climLen,
                      "fluxes.eventWoodC", woodC / climLen,
                      "fluxes.eventFineRootC", fineRootC / climLen,
                      "fluxes.eventCoarseRootC", coarseRootC / climLen);
      } break;
      case HARVEST: {
        // Harvest can both remove biomass and move biomass to the litter pool
        const HarvestParams *harvParams = gEvent->eventParams;
        const double fracRA = harvParams->fractionRemovedAbove;
        const double fracTA = harvParams->fractionTransferredAbove;
        const double fracRB = harvParams->fractionRemovedBelow;
        const double fracTB = harvParams->fractionTransferredBelow;

        // Litter increase
        const double litterAdd = fracTA * (envi.plantLeafC + envi.plantWoodC) +
                                 fracTB * (envi.fineRootC + envi.coarseRootC);
        // Pool reductions, counting both mass moved to litter and removed by
        // the harvest itself. Above-ground changes:
        const double leafDelta = -envi.plantLeafC * (fracRA + fracTA);
        const double woodDelta = -envi.plantWoodC * (fracRA + fracTA);
        // Below-ground changes:
        const double fineDelta = -envi.fineRootC * (fracRB + fracTB);
        const double coarseDelta = -envi.coarseRootC * (fracRB + fracTB);

        // Pool updates:
        fluxes.eventLitterC += litterAdd / climLen;
        fluxes.eventLeafC += leafDelta / climLen;
        fluxes.eventWoodC += woodDelta / climLen;
        fluxes.eventFineRootC += fineDelta / climLen;
        fluxes.eventCoarseRootC += coarseDelta / climLen;

        // FUTURE: move/remove biomass in N pools
        writeEventOut(gEvent, 5, "fluxes.eventLitterC", litterAdd / climLen,
                      "fluxes.eventLeafC", leafDelta / climLen,
                      "fluxes.eventWoodC", woodDelta / climLen,
                      "fluxes.eventFineRootC", fineDelta / climLen,
                      "fluxes.eventCoarseRootC", coarseDelta / climLen);
      } break;
      case TILLAGE: {
        // BIG NOTE: this is the one event type that is NOT modeled as a flux;
        // see updateEventTrackers() for more
        const TillageParams *tillParams = gEvent->eventParams;
        // Update the tillage mod for R_H calculations; this will be slowly
        // reduced by an exponential decay function. Note we add here, not set,
        // as there may be lingering effects from a prior tillage.
        eventTrackers.d_till_mod += tillParams->tillageEffect;
        writeEventOut(gEvent, 1, "eventTrackers.d_till_mod",
                      tillParams->tillageEffect);
      } break;
      case FERTILIZATION: {
        const FertilizationParams *fertParams = gEvent->eventParams;
        // const double orgN = fertParams->orgN;
        const double orgC = fertParams->orgC;
        // const double minN = fertParams->minN;

        fluxes.eventLitterC += orgC / climLen;

        // FUTURE: allocate to N pools

        // This will (likely) be 3 params eventually
        writeEventOut(gEvent, 1, "fluxes.eventLitterC", orgC / climLen);
      } break;
      default:
        logError("Unknown event type (%d) in processEvents()\n", gEvent->type);
        exit(EXIT_CODE_UNKNOWN_EVENT_TYPE_OR_PARAM);
    }

    gEvent = gEvent->nextEvent;
  }
}

void updatePoolsForEvents(void) {
  // Harvest and planting events
  envi.plantWoodC += fluxes.eventWoodC * climate->length;
  envi.plantLeafC += fluxes.eventLeafC * climate->length;

  // Irrigation events
  envi.soilWater += fluxes.eventSoilWater * climate->length;

  // Harvest and fertilization events
  if (ctx.litterPool) {
    envi.litter += fluxes.eventLitterC * climate->length;
  } else {
    envi.soil += fluxes.eventLitterC * climate->length;
  }

  // Harvest and planting events
  envi.coarseRootC += fluxes.eventCoarseRootC * climate->length;
  envi.fineRootC += fluxes.eventFineRootC * climate->length;
}

void freeEventList(void) {
  EventNode *curr, *prev;

  curr = gEvents;
  while (curr != NULL) {
    prev = curr;
    curr = curr->nextEvent;
    free(prev);
  }
}

// Definition of global event trackers struct
EventTrackers eventTrackers;

void initEventTrackers(void) { eventTrackers.d_till_mod = 0.0; }

void updateEventTrackers(void) {
  const double climLen = climate->length;

  // Tillage: decay any existing tillage effects at end of step
  if (eventTrackers.d_till_mod > 0) {
    eventTrackers.d_till_mod *= exp(-climLen * TILLAGE_DECAY_FACTOR);

    if (eventTrackers.d_till_mod < TILLAGE_THRESHOLD) {
      eventTrackers.d_till_mod = 0.0;
    }
  }
}

void printEvent(EventNode *oneEvent) {
  if (oneEvent == NULL) {
    return;
  }
  int year = oneEvent->year;
  int day = oneEvent->day;
  switch (oneEvent->type) {
    case IRRIGATION:
      printf("IRRIGATION on %d %d, ", year, day);
      IrrigationParams *const iParams =
          (IrrigationParams *)oneEvent->eventParams;
      printf("with params: amount added %4.2f\n", iParams->amountAdded);
      break;
    case FERTILIZATION:
      printf("FERTILIZATION on %d %d, ", year, day);
      FertilizationParams *const fParams =
          (FertilizationParams *)oneEvent->eventParams;
      printf("with params: org N %4.2f, org C %4.2f, min N %4.2f\n",
             fParams->orgN, fParams->orgC, fParams->minN);
      break;
    case PLANTING:
      printf("PLANTING on %d %d, ", year, day);
      PlantingParams *const pParams = (PlantingParams *)oneEvent->eventParams;
      printf("with params: leaf C %4.2f, wood C %4.2f, fine root C %4.2f, "
             "coarse root C %4.2f\n",
             pParams->leafC, pParams->woodC, pParams->fineRootC,
             pParams->coarseRootC);
      break;
    case TILLAGE:
      printf("TILLAGE on %d %d, ", year, day);
      TillageParams *const tParams = (TillageParams *)oneEvent->eventParams;
      printf("with params: tillageEffect %4.2f\n", tParams->tillageEffect);
      break;
    case HARVEST:
      printf("HARVEST on %d %d, ", year, day);
      HarvestParams *const hParams = (HarvestParams *)oneEvent->eventParams;
      printf("with params: frac removed above %4.2f, frac removed below %4.2f, "
             "frac transferred above %4.2f, frac transferred below %4.2f\n",
             hParams->fractionRemovedAbove, hParams->fractionRemovedBelow,
             hParams->fractionTransferredAbove,
             hParams->fractionTransferredBelow);
      break;
    default:
      printf("ERROR printing oneEvent: unknown type %d\n", oneEvent->type);
  }
}

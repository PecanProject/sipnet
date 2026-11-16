// clang-format off
/**
 * @file events.c
 * @brief Handles reading, parsing, and storing agronomic events for SIPNET simulations.
 *
 * The configured events input file specifies agronomic events. See the input
 * documentation (currently parameters.md) for information on that format.
 * Also, see test examples in `tests/sipnet/test_events_infrastructure/` and
 * `tests/sipnet/test_events_types/`.
 */
// clang-format on

#include "events.h"

#include "limitations.h"
#include "nitrogen.h"

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>  // for access()

#include "common/exitCodes.h"
#include "common/logging.h"
#include "common/util.h"

#include "state.h"

#define EVENT_LINE_SIZE 1024

// Global event variables - definition
static EventNode *gEvents = NULL;
static EventNode *gEvent = NULL;

// events.out handle, only needed here
static FILE *eventOutFile = NULL;

static EventNode *createEventNode(int year, int day, int eventType,
                                  const char *eventParamsStr) {
  static int nitrogenWarned = 0;
  EventNode *newEvent = (EventNode *)malloc(sizeof(EventNode));
  newEvent->year = year;
  newEvent->day = day;
  newEvent->type = eventType;
  newEvent->numLogParamPairs = 0;
  newEvent->logLine = dsCreate(0);

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
        logError("invalid harvest newEvent for year %d day %d; above and below "
                 "must each add to 1 or less\n",
                 year, day);
        exit(EXIT_CODE_BAD_PARAMETER_VALUE);
      }
      if (fracRA < 0.0 || fracRB < 0.0 || fracTA < 0.0 || fracTB < 0.0) {
        logError("invalid harvest newEvent for year %d day %d; fractions must "
                 "be non-negative\n",
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

      if (!ctx.nitrogenCycle && (orgN > 0.0 || minN > 0.0) && !nitrogenWarned) {
        logInfo("Fertilization nitrogen quantities are being ignored since "
                "nitrogen cycle modeling is off\n");
        nitrogenWarned = 1;
      }
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
    case LEAFON: {
      double dummy;
      LeafOnParams *lParams = (LeafOnParams *)malloc(sizeof(LeafOnParams));
      // Check for extraneous data: leafon takes no parameters, so error if any
      // numbers are found. sscanf returns 0 or EOF (-1) for empty/whitespace.
      int numRead = sscanf(eventParamsStr,  // NOLINT
                           "%lf", &dummy);
      if (numRead > NUM_LEAFON_PARAMS) {
        logError("parsing LeafOn params for year %d day %d\n", year, day);
        exit(EXIT_CODE_INPUT_FILE_ERROR);
      }
      newEvent->eventParams = lParams;
    } break;
    case LEAFOFF: {
      double dummy;
      LeafOffParams *lParams = (LeafOffParams *)malloc(sizeof(LeafOffParams));
      // Check for extraneous data: leafoff takes no parameters, so error if
      // any numbers are found. sscanf returns 0 or EOF (-1) for empty input.
      int numRead = sscanf(eventParamsStr,  // NOLINT
                           "%lf", &dummy);
      if (numRead > NUM_LEAFOFF_PARAMS) {
        logError("parsing LeafOff params for year %d day %d\n", year, day);
        exit(EXIT_CODE_INPUT_FILE_ERROR);
      }
      newEvent->eventParams = lParams;
    } break;
    case PLANTDEATH: {
      logError("PLANTDEATH event found for year %d day %d, but not implemented "
               "as an input event; please remove and re-run\n",
               year, day);
      exit(EXIT_CODE_INPUT_FILE_ERROR);
    }  // break;
    default:
      // Unknown type, error and exit
      logError("found unknown event type %d while reading event file\n",
               eventType);
      exit(EXIT_CODE_UNKNOWN_EVENT_TYPE_OR_PARAM);
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
    case LEAFON:
      return "leafon";
    case LEAFOFF:
      return "leafoff";
    case PLANTDEATH:
      return "plantdeath";
    default:
      logError("unknown event type in eventTypeToString (%d)", type);
      exit(EXIT_CODE_UNKNOWN_EVENT_TYPE_OR_PARAM);
  }
}

event_type_t eventStringToType(const char *eventTypeStr) {
  if (strcmp(eventTypeStr, "irrig") == 0) {
    return IRRIGATION;
  }
  if (strcmp(eventTypeStr, "fert") == 0) {
    return FERTILIZATION;
  }
  if (strcmp(eventTypeStr, "plant") == 0) {
    return PLANTING;
  }
  if (strcmp(eventTypeStr, "till") == 0) {
    return TILLAGE;
  }
  if (strcmp(eventTypeStr, "harv") == 0) {
    return HARVEST;
  }
  if (strcmp(eventTypeStr, "leafon") == 0) {
    return LEAFON;
  }
  if (strcmp(eventTypeStr, "leafoff") == 0) {
    return LEAFOFF;
  }
  if (strcmp(eventTypeStr, "plantdeath") == 0) {
    return PLANTDEATH;
  }
  return UNKNOWN_EVENT;
}

/**
 * Check if an event line was truncated during reading.
 * @param line The line buffer that was read from the file
 * @param len The length of the line string
 */
static void checkEventLineTruncation(const char *line, size_t len) {
  if (len == EVENT_LINE_SIZE - 1 && line[len - 1] != '\n') {
    logError("Event line too long (exceeds %d chars), data may be truncated\n",
             EVENT_LINE_SIZE);
    exit(EXIT_CODE_INPUT_FILE_ERROR);
  }
}

struct EventValidation {
  int todayHasHarvest;
  int todayHasPlanting;
  int hasCheckedLeafEvents;
};

static struct EventValidation eVal = {0};

void validateEvent(EventNode *event, int currYear, int currDay) {
  // logInfo("Validating event with type %s\n", eventTypeToString(event->type));
  // If we have a leaf event, make sure we aren't also looking for
  // calculated leaf events.
  if (event->type == LEAFOFF || event->type == LEAFON) {
    // logInfo("-->hasCheckedLeafEvents: %d\n", eVal.hasCheckedLeafEvents);
    if (!eVal.hasCheckedLeafEvents) {

      // logInfo("-->if test: %d\n",
      // ctx.gdd || ctx.soilPhenol || params.leafOnDay > 0 ||
      //   params.leafOffDay > 0);

      if (ctx.gdd || ctx.soilPhenol || params.leafOnDay > 0 ||
          params.leafOffDay > 0) {
        logError("calculated leaf events (via leafOnDay/leafOffDay params or "
                 "gdd/soil-phenol command-line options) are not compatible "
                 "with user-specified leaf events in event file\n");
        exit(EXIT_CODE_BAD_PARAMETER_VALUE);
      }
      eVal.hasCheckedLeafEvents = 1;
    }
    return;
  }

  // Make sure we don't have planting and harvest on the same day
  if (event->year > currYear || event->day > currDay) {
    eVal.todayHasHarvest = 0;
    eVal.todayHasPlanting = 0;
  }
  if (event->type == HARVEST) {
    eVal.todayHasHarvest = 1;
  }
  if (event->type == PLANTING) {
    eVal.todayHasPlanting = 1;
  }
  if (eVal.todayHasHarvest && eVal.todayHasPlanting) {
    logError("reading event file: planting and harvest events cannot occur "
             "on the same day (%d, %d)\n",
             event->year, event->day);
    exit(EXIT_CODE_INPUT_FILE_ERROR);
  }
}

EventNode *readEventData(const char *eventFile) {
  int year, day, eventType;
  int currYear, currDay;
  int numBytes;
  char *eventParamsStr;
  char eventTypeStr[20];
  char line[EVENT_LINE_SIZE];
  EventNode *curr, *next;
  EventNode *newEvent = NULL;

  // Check for a non-empty file
  if (access(eventFile, F_OK) != 0) {
    // no file found, which is fine; we're done, a vector of NULL is what we
    // want for newEvents
    logInfo("No event file found, assuming no input events\n");
    return newEvent;
  }

  logInfo("Begin reading event data from file %s\n", eventFile);

  FILE *in = openFile(eventFile, "r");
  eVal = (struct EventValidation){0};

  if (fgets(line, EVENT_LINE_SIZE, in) == NULL) {
    // Again, this is fine - just return the empty newEvents array
    return newEvent;
  }

  // Check for line truncation
  size_t len = strlen(line);
  checkEventLineTruncation(line, len);

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

  newEvent = createEventNode(year, day, eventType, eventParamsStr);
  next = newEvent;
  currYear = year;
  currDay = day;

  validateEvent(newEvent, currYear, currDay);

  while (fgets(line, EVENT_LINE_SIZE, in) != NULL) {
    // Check if line was truncated
    len = strlen(line);
    checkEventLineTruncation(line, len);
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
      logError("reading event file: last event was at (%d, %d), next event is "
               "at (%d, %d)\n",
               currYear, currDay, year, day);
      logError("event records must be in time-ascending order\n");
      exit(EXIT_CODE_INPUT_FILE_ERROR);
    }

    next = createEventNode(year, day, eventType, eventParamsStr);
    validateEvent(newEvent, currYear, currDay);

    curr->nextEvent = next;
    currYear = year;
    currDay = day;
  }

  fclose(in);
  return newEvent;
}

void openEventOutFile(const char *eventOutFilePath, int printHeader) {
  eventOutFile = openFile(eventOutFilePath, "w");
  if (printHeader) {
    // Use format string analogous to the one in writeEventsOut for
    // better alignment (won't be perfect, but definitely better)
    fprintf(eventOutFile, "%4s  %3s  %-7s  %s", "year", "day", "type",
            "param_name=delta[,param_name=delta,...]\n");
  }
}

void appendLog(EventNode *event, int numParams, ...) {
  va_list args;
  va_start(args, numParams);
  for (int ind = 0; ind < numParams; ind++) {
    char *param = va_arg(args, char *);
    double val = va_arg(args, double);
    int success = dsAppendFormatted(event->logLine, "%s=%-.2f,", param, val);
    if (!success) {
      logError("appending event log line failed\n");
      exit(EXIT_CODE_INTERNAL_ERROR);
    }
  }
  va_end(args);
}

void writePendingLeafOnEvent(EventNode *event) {
  const double climLen = climate->length;
  double creation = fluxes.eventLeafOnCreation * climLen;
  double creationFromWood = fluxes.eventLeafOnCreationFromWood * climLen;
  // clang-format off
  appendLog(event, 2,
    "eventLeafOnCreation", creation,
    "eventLeafOnCreationFromWood", creationFromWood);
  // clang-format on

  // No N changes for leaf on (N is implicitly handled for biomass pools)
}

void writePendingLeafOffEvent(EventNode *event) {
  const double climLen = climate->length;
  double litterC = fluxes.eventLeafOffLitterC * climLen;
  appendLog(event, 1, "eventLeafOffLitterC", litterC);
  if (ctx.nitrogenCycle) {
    double litterN = fluxes.eventLeafOffLitterN * climLen;
    double leafOffNResorp = fluxes.eventLeafOffNResorption * climLen;
    // clang-format off
    appendLog(event, 2,
    "eventLeafOffNResorption", leafOffNResorp,
    "eventLeafOffLitterN", litterN);
    // clang-format on
  }
}

void writePendingHarvestEvent(EventNode *event) {
  double climLen = climate->length;
  double litterAdd = fluxes.harvestLitterC * climLen;
  double soilAdd = fluxes.harvestSoilC * climLen;
  double leafDelta = fluxes.eventLeafC * climLen;
  double woodDelta = fluxes.eventWoodC * climLen;
  double accountingDelta = fluxes.eventAccountingC * climLen;
  double fineDelta = fluxes.eventFineRootC * climLen;
  double coarseDelta = fluxes.eventCoarseRootC * climLen;
  double outputC = fluxes.eventOutputC * climLen;
  // clang-format off
  appendLog(
      event, 8,
      "eventSoilC", soilAdd,  // also modified by fert
      "eventLitterC", litterAdd,  // fert
      "eventLeafC", leafDelta,  // planting
      "eventWoodC", woodDelta,  // planting
      "eventAccountingC", accountingDelta,
      "eventFineRootC", fineDelta,  // planting
      "eventCoarseRootC", coarseDelta,  // planting
      "eventOutputC", outputC);
  // clang-format on
  if (ctx.nitrogenCycle) {
    double soilNAdd = fluxes.eventSoilOrgN * climLen;
    double litterNAdd = fluxes.harvestLitterN * climLen;
    double outputN = fluxes.eventOutputN * climLen;
    // clang-format off
    appendLog(
        event, 3,
        "eventSoilOrgN", soilNAdd,
        "eventLitterN", litterNAdd,  // fert
        "eventOutputN", outputN);
    // clang-format on
  }
}

static int eventIsDue(const EventNode *event, int year, int day) {
  return event->year < year || (event->year == year && event->day <= day);
}

void writeEventsOut(void) {
  // We move the global events pointer here (instead of a copy as in the
  // processEvents functions), as this is the last pass
  // We will also handle any "pending" events (currently: leaf-off) whose
  // values may have changed due to limitation checks

  const int climYear = climate->year;
  const int climDay = climate->day;
  int hasSeenLeafOn = 0;
  int hasSeenLeafOff = 0;
  int hasSeenHarvest = 0;
  while (gEvent != NULL && eventIsDue(gEvent, climYear, climDay)) {
    // First up, check the pending events
    if (gEvent->type == HARVEST) {
      if (hasSeenHarvest) {
        gEvent = gEvent->nextEvent;
        continue;
      }
      writePendingHarvestEvent(gEvent);
      hasSeenHarvest = 1;
    }
    if (gEvent->type == LEAFON) {
      if (hasSeenLeafOn) {
        gEvent = gEvent->nextEvent;
        continue;
      }
      writePendingLeafOnEvent(gEvent);
      hasSeenLeafOn = 1;
    }
    if (gEvent->type == LEAFOFF) {
      if (hasSeenLeafOff) {
        gEvent = gEvent->nextEvent;
        continue;
      }
      writePendingLeafOffEvent(gEvent);
      hasSeenLeafOff = 1;
    }

    // Change last char to a newline if it is a comma
    // Get the current length of the string
    char *log = gEvent->logLine->buffer;
    size_t len = strlen(log);
    if (len > 0 && log[len - 1] == ',') {
      log[len - 1] = '\n';
    } else {
      dsAppend(gEvent->logLine, "\n");
    }
    fprintf(eventOutFile, "%4d  %3d  %-7s  %s", gEvent->year, gEvent->day,
            eventTypeToString(gEvent->type), log);
    gEvent = gEvent->nextEvent;
  }

  // Last, check for calculated leaf on/off "events"
  double climLen = climate->length;
  if (fluxes.leafOnCreation > 0.0) {
    // clang-format off
    writeComputedEventOut(
      climate->year, climate->day, eventTypeToString(LEAFON), 2,
      "leafOnCreation", fluxes.leafOnCreation * climLen,
      "leafOnCreationFromWood", fluxes.leafOnCreationFromWood * climLen);
    // clang-format on
  }
  if (fluxes.leafOffLitter > 0.0) {
    // clang-format off
    writeComputedEventOut(
      climate->year, climate->day, eventTypeToString(LEAFOFF), 1,
      "leafOffLitter", fluxes.leafOffLitter * climLen);
    // clang-format on
  }
}

void writeComputedEventOut(int year, int day, const char *type, int numParams,
                           ...) {
  va_list args;
  va_start(args, numParams);
  // Standard prefix for all
  fprintf(eventOutFile, "%4d  %3d  %-7s  ", year, day, type);

  // Variable output per oneEvent type
  for (int ind = 0; ind < numParams; ind++) {
    char *param = va_arg(args, char *);
    double val = va_arg(args, double);
    char suffix = (ind == numParams - 1) ? '\n' : ',';
    fprintf(eventOutFile, "%s=%-.2f%c", param, val, suffix);
  }

  va_end(args);
}

void closeEventOutFile() {
  if (eventOutFile) {
    fclose(eventOutFile);
    eventOutFile = NULL;
  }
}

void initEvents(const char *eventInFile, const char *eventOutFilePath,
                int printHeader) {
  if (ctx.events) {
    gEvents = readEventData(eventInFile);
    openEventOutFile(eventOutFilePath, printHeader);
  }
}

void setupEvents() { gEvent = gEvents; }

int isFirstEventBefore(int year, int day) {
  if (gEvents == NULL) {
    // No events, so nothing to check
    return 0;
  }
  EventNode *firstEvent = gEvents;
  if (firstEvent->year != year) {
    return firstEvent->year < year;
  }
  return firstEvent->day < day;
}

EventNode *getCurrentEvent(void) { return gEvent; }

void processEventsForCarbon(EventNode *event) {
  // Event fluxes have all been reset to zero at the start of the time step,
  // so we can just add to them as needed

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

  // Reset harvest tracking
  eventTrackers.harvestTrackers = (HarvestTrackers){0};

  // Make sure we don't have more than 100% leaves fall for leaf-off
  double totalLeafFallFrac = 0.0;

  int hasSeenLeafOn = 0;
  int hasSeenLeafOff = 0;
  int hasSeenHarvest = 0;

  while (event != NULL && eventIsDue(event, climYear, climDay)) {
    // The events file has been tested on read, so we know this event list
    // should be in chrono order. However, we need to check to make sure the
    // current event is not in the past, as that would indicate an event that
    // did not have a corresponding climate file record.
    if (event->year < climYear || event->day < climDay) {
      logError("Agronomic event found for year: %d day: %d that does not "
               "have a corresponding record in the climate file "
               "(current climate date is year %d day %d)\n",
               event->year, event->day, climYear, climDay);
      exit(EXIT_CODE_INPUT_FILE_ERROR);
    }

    switch (event->type) {
      case IRRIGATION: {
        const IrrigationParams *irrParams = event->eventParams;
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

        appendLog(event, 2, "eventSoilWater", soilAmount, "eventEvap",
                  evapAmount);

      } break;
      case PLANTING: {
        const PlantingParams *plantParams = event->eventParams;
        const double leafC = plantParams->leafC;
        const double woodC = plantParams->woodC;
        const double fineRootC = plantParams->fineRootC;
        const double coarseRootC = plantParams->coarseRootC;

        // Update the fluxes
        fluxes.eventLeafC += leafC / climLen;
        fluxes.eventWoodC += woodC / climLen;
        fluxes.eventFineRootC += fineRootC / climLen;
        fluxes.eventCoarseRootC += coarseRootC / climLen;

        // No need to allocate to biomass N pools, we don't track that N
        // explicitly

        // MASS BALANCE: this is a system input
        const double inputC = leafC + woodC + fineRootC + coarseRootC;
        fluxes.eventInputC += inputC / climLen;

        // clang-format off
        appendLog(event, 5,
                      "eventLeafC", leafC,
                      "eventWoodC", woodC,
                      "eventFineRootC", fineRootC,
                      "eventCoarseRootC", coarseRootC,
                      "eventInputC", inputC);
        // clang-format on

      } break;
      case HARVEST: {
        if (hasSeenHarvest) {
          logInfo("Multiple harvest events found for year %d day %d; reporting "
                  "will be combined into one event line\n",
                  event->year, event->day);
        }
        hasSeenHarvest = 1;

        // Harvest can both remove biomass and move biomass to the soil/litter
        // pools
        const HarvestParams *harvParams = event->eventParams;
        const double fracRA = harvParams->fractionRemovedAbove;
        const double fracRB = harvParams->fractionRemovedBelow;
        const double fracTA = harvParams->fractionTransferredAbove;
        const double fracTB = harvParams->fractionTransferredBelow;
        const double woodC = envi.plantWoodC + envi.plantCAccountingDelta;

        // Record fraction of total biomass removed and transferred
        double aboveMass = woodC + envi.plantLeafC;
        double belowMass = envi.fineRootC + envi.coarseRootC;
        double totalMass = aboveMass + belowMass;
        HarvestTrackers *ht = &eventTrackers.harvestTrackers;
        if (totalMass > TINY) {
          double massRemoved = fracRA * aboveMass + fracRB * belowMass;
          double massTransferred = fracTA * aboveMass + fracTB * belowMass;
          ht->totalFracRemoved += massRemoved / totalMass;
          ht->totalFracTransferred += massTransferred / totalMass;
          ht->totalFracRemovedAbove += fracRA;
          ht->totalFracRemovedBelow += fracRB;
          ht->totalFracTransferredAbove += fracTA;
          ht->totalFracTransferredBelow += fracTB;
          if (ht->totalFracRemovedAbove + ht->totalFracTransferredAbove >
                  1.0 + TINY ||
              ht->totalFracRemovedBelow + ht->totalFracTransferredBelow >
                  1.0 + TINY) {
            logError("Harvest event(s) at year %d day %d has total above-ground"
                     " or below-ground removal + transfer fraction > 1.0"
                     " (above %.3f, below %.3f)\n",
                     event->year, event->day,
                     ht->totalFracRemovedAbove + ht->totalFracTransferredAbove,
                     ht->totalFracRemovedBelow + ht->totalFracTransferredBelow);
            exit(EXIT_CODE_BAD_PARAMETER_VALUE);
          }
        } else {
          logWarning("Harvest event at year %d day %d has no biomass to remove "
                     "or transfer\n",
                     event->year, event->day);
        }

        // Litter increase
        double litterAdd = fracTA * (envi.plantLeafC + woodC);
        double soilAdd = fracTB * (envi.fineRootC + envi.coarseRootC);

        // Pool reductions, counting both mass moved to litter and removed by
        // the harvest itself. Above-ground changes:
        const double leafDelta = -envi.plantLeafC * (fracRA + fracTA);
        const double woodDelta = -envi.plantWoodC * (fracRA + fracTA);
        const double accountingDelta =
            -envi.plantCAccountingDelta * (fracRA + fracTA);
        // Below-ground changes:
        const double fineDelta = -envi.fineRootC * (fracRB + fracTB);
        const double coarseDelta = -envi.coarseRootC * (fracRB + fracTB);

        // Pool updates:
        if (!ctx.litterPool) {
          // send it all to the soil
          soilAdd += litterAdd;
          litterAdd = 0.0;
        }
        fluxes.harvestLitterC += litterAdd / climLen;
        fluxes.harvestSoilC += soilAdd / climLen;
        fluxes.eventLeafC += leafDelta / climLen;
        fluxes.eventWoodC += woodDelta / climLen;
        fluxes.eventAccountingC += accountingDelta / climLen;
        fluxes.eventFineRootC += fineDelta / climLen;
        fluxes.eventCoarseRootC += coarseDelta / climLen;

        // MASS BALANCE: removed fractions are system outputs
        const double outputC = ((woodC + envi.plantLeafC) * fracRA +
                                (envi.fineRootC + envi.coarseRootC) * fracRB);
        fluxes.eventOutputC += outputC / climLen;

        // No log here, as eventLeafC may change due to limitation checks
      } break;
      case TILLAGE: {
        // BIG NOTE: this is the one event type that is NOT modeled as a flux;
        // see updateEventTrackers() for more
        const TillageParams *tillParams = event->eventParams;
        // Update the tillage mod for R_H calculations; this will be slowly
        // reduced by an exponential decay function. Note we add here, not set,
        // as there may be lingering effects from a prior tillage.
        eventTrackers.d_till_mod += tillParams->tillageEffect;

        appendLog(event, 1, "eventTrackers.d_till_mod",
                  tillParams->tillageEffect);

      } break;
      case FERTILIZATION: {
        const FertilizationParams *fertParams = event->eventParams;
        const double orgC = fertParams->orgC;
        if (ctx.litterPool) {
          fluxes.eventLitterC += orgC / climLen;
        } else {
          fluxes.eventSoilC += orgC / climLen;
        }

        // MASS BALANCE: this is a system input
        fluxes.eventInputC += orgC / climLen;

        // clang-format off
        appendLog(event, 3,
          "eventLitterC", ctx.litterPool ? orgC : 0.0,
          "eventSoilC", ctx.litterPool ? 0.0 : orgC,
          "eventInputC", orgC);
        // clang-format on

      } break;
      case LEAFON: {
        if (hasSeenLeafOn) {
          logInfo("Multiple leaf-on events found for year %d day %d; reporting "
                  "will be combined into one event line\n",
                  event->year, event->day);
        }
        hasSeenLeafOn = 1;

        double leafOnFlux = params.leafGrowth / climLen;
        checkLeafOnLimitation(&leafOnFlux);
        fluxes.eventLeafOnCreation += leafOnFlux;
        double totalSourceC = envi.plantWoodC + envi.coarseRootC;
        if (totalSourceC > TINY) {
          double leafOnFluxFromWood =
              leafOnFlux * envi.plantWoodC / totalSourceC;
          fluxes.eventLeafOnCreationFromWood += leafOnFluxFromWood;
        }
        // Unlike planting, this is NOT a system input, so no adjustments to
        // eventInputC

        // No log here, in case we have more than one
      } break;
      case LEAFOFF: {
        if (hasSeenLeafOff) {
          logInfo(
              "Multiple leaf-off events found for year %d day %d; reporting "
              "will be combined into one event line\n",
              event->year, event->day);
        }
        hasSeenLeafOff = 1;

        double frac = params.fracLeafFall;
        totalLeafFallFrac += frac;
        if (totalLeafFallFrac > 1.0) {
          logError("Total leaf fall for leaf-off event(s) is greater than 100 "
                   "percent (%.3f) for year %d day %d\n",
                   totalLeafFallFrac, event->year, event->day);
          exit(EXIT_CODE_BAD_PARAMETER_VALUE);
        }

        double leafOff = envi.plantLeafC * params.fracLeafFall;
        fluxes.eventLeafOffLitterC += leafOff / climLen;
        // No log here, as this value may change due to limitation checks
      } break;
      case PLANTDEATH:
        // There should be no way to get here, but covering our bases...
        logWarning("PLANTDEATH event found for year %d day %d, but not "
                   "implemented as an input event; ignoring\n",
                   event->year, event->day);
        break;
      default:
        logError("Unknown event type (%d) in processEvents()\n", event->type);
        exit(EXIT_CODE_UNKNOWN_EVENT_TYPE_OR_PARAM);
    }

    event = event->nextEvent;
  }
}

void processEventsForNitrogen(EventNode *event) {
  if (!ctx.nitrogenCycle) {
    return;
  }

  // Event fluxes have all been reset to zero at the start of the time step,
  // so we can just add to them as needed

  // If event starts off NULL, this function will just fall through, as it
  // should.
  const int climYear = climate->year;
  const int climDay = climate->day;
  const double climLen = climate->length;

  // Checks performed in processEventsForCarbon are not repeated here unless
  // necessary for nitrogen handling.

  // Make sure not to double-process these event types, in case there is more
  // than one
  int hasSeenLeafOff = 0;
  int hasSeenHarvest = 0;

  while (event != NULL && eventIsDue(event, climYear, climDay)) {
    switch (event->type) {
      case IRRIGATION: {
        // Nothing to do for irrigation events
      } break;
      case PLANTING: {
        const PlantingParams *plantParams = event->eventParams;
        const double leafC = plantParams->leafC;
        const double woodC = plantParams->woodC;
        const double fineRootC = plantParams->fineRootC;
        const double coarseRootC = plantParams->coarseRootC;

        // No need to allocate to biomass N pools, we don't track that N
        // explicitly

        // MASS BALANCE: this is a system input
        double inputN = leafC / params.leafCN + woodC / params.woodCN +
                        fineRootC / params.fineRootCN +
                        coarseRootC / params.woodCN;
        fluxes.eventInputN += inputN / climLen;

        appendLog(event, 1, "eventInputN", inputN);
      } break;
      case HARVEST: {
        if (hasSeenHarvest) {
          break;  // already processed the combined harvest events
        }
        hasSeenHarvest = 1;

        // Harvest can both remove biomass and move biomass to the soil/litter
        // pools
        HarvestTrackers *ht = &eventTrackers.harvestTrackers;
        const double fracRA = ht->totalFracRemovedAbove;
        const double fracRB = ht->totalFracRemovedBelow;
        const double fracTA = ht->totalFracTransferredAbove;
        const double fracTB = ht->totalFracTransferredBelow;

        // No need to allocate to biomass N pools, we don't track that N
        // explicitly. We do need to handle soil and litter N, though.
        // Note: ctx.nitrogenCycle implies ctx.litterPool
        // Litter N increase
        const double totalAbove = (envi.plantLeafC / params.leafCN) +
                                  (envi.plantWoodC / params.woodCN);
        const double totalBelow = (envi.fineRootC / params.fineRootCN) +
                                  (envi.coarseRootC / params.woodCN);
        double litterNAdd = fracTA * totalAbove;
        double soilNAdd = fracTB * totalBelow;
        fluxes.eventSoilOrgN += soilNAdd / climLen;
        fluxes.harvestLitterN += litterNAdd / climLen;

        // MASS BALANCE: removed fractions are system outputs
        // just plantWoodC here, not woodC
        double outputN = (envi.plantWoodC / params.woodCN +
                          envi.plantLeafC / params.leafCN) *
                             fracRA +
                         (envi.fineRootC / params.fineRootCN +
                          envi.coarseRootC / params.woodCN) *
                             fracRB;
        fluxes.eventOutputN += outputN / climLen;

        // NOTE: leaf/litter/output C may have been adjusted by
        // limitation checks; make sure to do the same for N
        double leafDiffFlux = ht->leafCAdjustment;

        // Figure out how to distribute
        double litterFlux, outputFlux;
        partitionLeafAdjustment(leafDiffFlux, &litterFlux, &outputFlux);
        fluxes.harvestLitterN -= litterFlux / params.leafCN;
        fluxes.eventOutputN -= outputFlux / params.leafCN;
      } break;
      case TILLAGE: {
        // Nothing to do for tillage events
      } break;
      case FERTILIZATION: {
        const FertilizationParams *fertParams = event->eventParams;
        double orgN = fertParams->orgN;
        double minN = fertParams->minN;
        // As the warning says in readEventData(), we ignore N when the
        // nitrogen cycle model is off
        // Implies ctx.litterPool
        fluxes.eventLitterN += orgN / climLen;
        fluxes.eventMinN += minN / climLen;

        // MASS BALANCE: this is a system input
        fluxes.eventInputN += (orgN + minN) / climLen;

        // clang-format off
        appendLog(event, 3,
          "eventMinN", minN,
          "eventLitterN", orgN,
          "eventInputN", (orgN + minN));
        // clang-format on
      } break;
      case LEAFON: {
        // Nitrogen is handled implicitly by relative CN ratios. Missing N
        // from low-N wood to higher-N leaves is accounted for in
        // calcNFixationAndUptakeFluxes() via calcPlantNDemandFlux()

        // Unlike planting, this is NOT a system input, so no adjustments to
        // eventInputN
      } break;
      case LEAFOFF: {
        if (hasSeenLeafOff) {
          break;  // already processed the combined leaf-off events
        }
        hasSeenLeafOff = 1;

        // We need to use fluxes.eventLeafOffLitter here instead of
        // recalculating it, as it may have been reduced.

        // Nitrogen - need to account for leaf N moving to litter, as with
        // harvests
        double leafOff = fluxes.eventLeafOffLitterC * climLen;
        calcLeafOffNEffects(leafOff, &fluxes.eventLeafOffNResorption,
                            &fluxes.eventLeafOffLitterN);

        // No log here, as these values may change
      } break;
      case PLANTDEATH:
        // Nothing to do here
        break;
      default:
        logError("Unknown event type (%d) in processEvents()\n", event->type);
        exit(EXIT_CODE_UNKNOWN_EVENT_TYPE_OR_PARAM);
    }

    event = event->nextEvent;
  }
}

void updatePoolsForEvents(void) {
  // CARBON
  // Harvest and planting events
  envi.plantWoodC += fluxes.eventWoodC * climate->length;
  envi.plantCAccountingDelta += fluxes.eventAccountingC * climate->length;
  envi.plantLeafC += fluxes.eventLeafC * climate->length;

  // Harvest and fertilization events
  envi.soilC += (fluxes.eventSoilC + fluxes.harvestSoilC) * climate->length;
  if (ctx.litterPool) {
    envi.litterC +=
        (fluxes.eventLitterC + fluxes.harvestLitterC) * climate->length;
  }

  // Leaf on and off events
  // Leaf on draws from wood and coarse root pools in proportion to their sizes
  envi.plantWoodC -= fluxes.eventLeafOnCreationFromWood * climate->length;
  double eventLeafOnCreationFromRoot =
      fluxes.eventLeafOnCreation - fluxes.eventLeafOnCreationFromWood;
  envi.coarseRootC -= eventLeafOnCreationFromRoot * climate->length;
  envi.plantLeafC += (fluxes.eventLeafOnCreation - fluxes.eventLeafOffLitterC) *
                     climate->length;
  if (ctx.litterPool) {
    envi.litterC += fluxes.eventLeafOffLitterC * climate->length;
  } else {
    envi.soilC += fluxes.eventLeafOffLitterC * climate->length;
  }

  // Harvest and planting events
  envi.coarseRootC += fluxes.eventCoarseRootC * climate->length;
  envi.fineRootC += fluxes.eventFineRootC * climate->length;

  // WATER
  // Irrigation events
  envi.soilWater += fluxes.eventSoilWater * climate->length;

  // NITROGEN
  // Harvest, fertilization, and leaf-off events
  // (Planting events don't explicitly handle N)
  // Note: nitrogen_cycle implies litter_pool
  if (ctx.nitrogenCycle) {
    envi.minN += fluxes.eventMinN * climate->length;
    envi.soilOrgN += fluxes.eventSoilOrgN * climate->length;
    envi.litterN += (fluxes.eventLitterN + fluxes.harvestLitterN +
                     fluxes.eventLeafOffLitterN) *
                    climate->length;
    double leafOnNFlux = calcLeafOnNFromC(fluxes.eventLeafOnCreation);
    envi.plantStorageN +=
        (fluxes.eventLeafOffNResorption - leafOnNFlux) * climate->length;
  }
}

void freeEventList(void) {
  EventNode *curr, *prev;

  curr = gEvents;
  while (curr != NULL) {
    prev = curr;
    curr = curr->nextEvent;
    if (prev->eventParams != NULL) {
      free(prev->eventParams);
    }
    if (prev->logLine != NULL) {
      dsFree(prev->logLine);
    }
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
    case LEAFON:
      printf("LEAFON on %d %d, ", year, day);
      // No real params for leafon
      break;
    case LEAFOFF:
      printf("LEAFOFF on %d %d, ", year, day);
      // No real params for leafoff
      break;
    case PLANTDEATH:
      printf("PLANTDEATH on %d %d, ", year, day);
      // No real params for plantdeath
      break;
    default:
      printf("ERROR printing oneEvent: unknown type %d\n", oneEvent->type);
  }
}

void partitionLeafAdjustment(double leafAdjust, double *litterAdjust,
                             double *outputAdjust) {
  const HarvestTrackers *ht = &eventTrackers.harvestTrackers;
  double fracRemovedAbove = ht->totalFracRemovedAbove;
  double fracTransferredAbove = ht->totalFracTransferredAbove;
  double totalFrac = fracRemovedAbove + fracTransferredAbove;
  if (totalFrac > 0.0) {
    *litterAdjust = leafAdjust * fracTransferredAbove / totalFrac;
    *outputAdjust = leafAdjust * fracRemovedAbove / totalFrac;
  } else {
    *litterAdjust = 0.0;
    *outputAdjust = 0.0;
  }
}

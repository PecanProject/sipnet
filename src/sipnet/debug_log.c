#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "debug_log.h"

#include "events.h"
#include "common/exitCodes.h"
#include "common/logging.h"
#include "common/context.h"
#include "common/util.h"
#include "state.h"

#define NUM_LOGGED_ENVI_FIELDS 13
#define NUM_LOGGED_FLUX_FIELDS 59
#define NUM_LOGGED_TRACKER_FIELDS 33
#define NUM_LOGGED_PHEN_TRACKER_FIELDS 3
#define NUM_LOGGED_SURVIVAL_FIELDS 1
#define NUM_LOGGED_EVENT_TRACKER_FIELDS 7

#define DEBUG_LAYOUT_ENVI_SIZE (8 * NUM_LOGGED_ENVI_FIELDS)
#define DEBUG_LAYOUT_FLUX_SIZE (8 * NUM_LOGGED_FLUX_FIELDS)
// The Trackers struct is not all doubles, but the int(s) get padded to 8 bytes
#define DEBUG_LAYOUT_TRACKER_SIZE (8 * NUM_LOGGED_TRACKER_FIELDS)
#define DEBUG_LAYOUT_PHEN_SIZE (4 * NUM_LOGGED_PHEN_TRACKER_FIELDS)
#define DEBUG_LAYOUT_SURVIVAL_SIZE (4 * NUM_LOGGED_SURVIVAL_FIELDS)
#define DEBUG_LAYOUT_EVENT_SIZE (8 * NUM_LOGGED_EVENT_TRACKER_FIELDS)

_Static_assert(sizeof(Envi) == DEBUG_LAYOUT_ENVI_SIZE,
               "Debug log schema drift: Envi changed; update debug_log.c");
_Static_assert(sizeof(Fluxes) == DEBUG_LAYOUT_FLUX_SIZE,
               "Debug log schema drift: Fluxes changed; update debug_log.c");
_Static_assert(sizeof(Trackers) == DEBUG_LAYOUT_TRACKER_SIZE,
               "Debug log schema drift: Trackers changed; update debug_log.c");
_Static_assert(
    sizeof(PhenologyTrackers) == DEBUG_LAYOUT_PHEN_SIZE,
    "Debug log schema drift: PhenologyTrackers changed; update debug_log.c");
_Static_assert(
    sizeof(PlantSurvivalTracker) == DEBUG_LAYOUT_SURVIVAL_SIZE,
    "Debug log schema drift: PlantSurvival changed; update debug_log.c");
_Static_assert(
    sizeof(EventTrackers) == DEBUG_LAYOUT_EVENT_SIZE,
    "Debug log schema drift: EventTrackers changed; update debug_log.c");

typedef enum DebugFieldType {
  DEBUG_FIELD_INT = 0,
  DEBUG_FIELD_DOUBLE = 1
} DebugFieldType;

typedef struct DebugField {
  const char *name;
  DebugFieldType type;
  const void *value;
} DebugField;

typedef struct DebugFieldArrays {
  DebugField enviDF[NUM_LOGGED_ENVI_FIELDS];
  DebugField fluxDF[NUM_LOGGED_FLUX_FIELDS];
  DebugField trackerDF[NUM_LOGGED_TRACKER_FIELDS];
  DebugField phenoDF[NUM_LOGGED_PHEN_TRACKER_FIELDS];
  DebugField survivalDF[NUM_LOGGED_SURVIVAL_FIELDS];
  DebugField eventDF[NUM_LOGGED_EVENT_TRACKER_FIELDS];
} DebugFieldArrays;

static DebugFieldArrays *debugFields = NULL;

void initDebugArrays() {
  if (strlen(ctx.debugLogPrefix) == 0) {
    return;
  }

  debugFields = malloc(sizeof(DebugFieldArrays));
  if (debugFields == NULL) {
    logError("memory allocation failure in debug log initialization\n");
    exit(EXIT_CODE_INTERNAL_ERROR);
  }
  int ind = 0;

  // clang-format off
  debugFields->enviDF[ind++] = (DebugField){"plantWoodC", DEBUG_FIELD_DOUBLE, &envi.plantWoodC},
  debugFields->enviDF[ind++] = (DebugField){"plantLeafC", DEBUG_FIELD_DOUBLE, &envi.plantLeafC},
  debugFields->enviDF[ind++] = (DebugField){"soilC", DEBUG_FIELD_DOUBLE, &envi.soilC},
  debugFields->enviDF[ind++] = (DebugField){"soilWater", DEBUG_FIELD_DOUBLE, &envi.soilWater},
  debugFields->enviDF[ind++] = (DebugField){"litterC", DEBUG_FIELD_DOUBLE, &envi.litterC},
  debugFields->enviDF[ind++] = (DebugField){"snow", DEBUG_FIELD_DOUBLE, &envi.snow},
  debugFields->enviDF[ind++] = (DebugField){"coarseRootC", DEBUG_FIELD_DOUBLE, &envi.coarseRootC},
  debugFields->enviDF[ind++] = (DebugField){"fineRootC", DEBUG_FIELD_DOUBLE, &envi.fineRootC},
  debugFields->enviDF[ind++] = (DebugField){"minN", DEBUG_FIELD_DOUBLE, &envi.minN},
  debugFields->enviDF[ind++] = (DebugField){"soilOrgN", DEBUG_FIELD_DOUBLE, &envi.soilOrgN},
  debugFields->enviDF[ind++] = (DebugField){"litterN", DEBUG_FIELD_DOUBLE, &envi.litterN},
  debugFields->enviDF[ind++] = (DebugField){"plantStorageN", DEBUG_FIELD_DOUBLE, &envi.plantStorageN},
  debugFields->enviDF[ind++] = (DebugField){"plantCAccountingDelta", DEBUG_FIELD_DOUBLE,&envi.plantCAccountingDelta};
  if (ind != NUM_LOGGED_ENVI_FIELDS) {
    logInternalError("Debug log array size mismatch: enviDF\n");
    exit(EXIT_CODE_INTERNAL_ERROR);
  }

  ind = 0;
  debugFields->fluxDF[ind++] = (DebugField){"photosynthesis", DEBUG_FIELD_DOUBLE, &fluxes.photosynthesis};
  debugFields->fluxDF[ind++] = (DebugField){"leafLitter", DEBUG_FIELD_DOUBLE, &fluxes.leafLitter},
  debugFields->fluxDF[ind++] = (DebugField){"woodLitter", DEBUG_FIELD_DOUBLE, &fluxes.woodLitter},
  debugFields->fluxDF[ind++] = (DebugField){"rVeg", DEBUG_FIELD_DOUBLE, &fluxes.rVeg},
  debugFields->fluxDF[ind++] = (DebugField){"rSoil", DEBUG_FIELD_DOUBLE, &fluxes.rSoil},
  debugFields->fluxDF[ind++] = (DebugField){"rain", DEBUG_FIELD_DOUBLE, &fluxes.rain},
  debugFields->fluxDF[ind++] = (DebugField){"transpiration", DEBUG_FIELD_DOUBLE, &fluxes.transpiration},
  debugFields->fluxDF[ind++] = (DebugField){"drainage", DEBUG_FIELD_DOUBLE, &fluxes.drainage},
  debugFields->fluxDF[ind++] = (DebugField){"litterToSoil", DEBUG_FIELD_DOUBLE, &fluxes.litterToSoil},
  debugFields->fluxDF[ind++] = (DebugField){"rLitter", DEBUG_FIELD_DOUBLE, &fluxes.rLitter},
  debugFields->fluxDF[ind++] = (DebugField){"snowFall", DEBUG_FIELD_DOUBLE, &fluxes.snowFall},
  debugFields->fluxDF[ind++] = (DebugField){"snowMelt", DEBUG_FIELD_DOUBLE, &fluxes.snowMelt},
  debugFields->fluxDF[ind++] = (DebugField){"sublimation", DEBUG_FIELD_DOUBLE, &fluxes.sublimation},
  debugFields->fluxDF[ind++] = (DebugField){"immedEvap", DEBUG_FIELD_DOUBLE, &fluxes.immedEvap},
  debugFields->fluxDF[ind++] = (DebugField){"fastFlow", DEBUG_FIELD_DOUBLE, &fluxes.fastFlow},
  debugFields->fluxDF[ind++] = (DebugField){"evaporation", DEBUG_FIELD_DOUBLE, &fluxes.evaporation},
  debugFields->fluxDF[ind++] = (DebugField){"fineRootLoss", DEBUG_FIELD_DOUBLE, &fluxes.fineRootLoss},
  debugFields->fluxDF[ind++] = (DebugField){"coarseRootLoss", DEBUG_FIELD_DOUBLE, &fluxes.coarseRootLoss},
  debugFields->fluxDF[ind++] = (DebugField){"fineRootCreation", DEBUG_FIELD_DOUBLE, &fluxes.fineRootCreation},
  debugFields->fluxDF[ind++] = (DebugField){"coarseRootCreation", DEBUG_FIELD_DOUBLE, &fluxes.coarseRootCreation},
  debugFields->fluxDF[ind++] = (DebugField){"rCoarseRoot", DEBUG_FIELD_DOUBLE, &fluxes.rCoarseRoot},
  debugFields->fluxDF[ind++] = (DebugField){"rFineRoot", DEBUG_FIELD_DOUBLE, &fluxes.rFineRoot},
  debugFields->fluxDF[ind++] = (DebugField){"leafCreation", DEBUG_FIELD_DOUBLE, &fluxes.leafCreation},
  debugFields->fluxDF[ind++] = (DebugField){"woodCreation", DEBUG_FIELD_DOUBLE, &fluxes.woodCreation},
  debugFields->fluxDF[ind++] = (DebugField){"leafOnCreation", DEBUG_FIELD_DOUBLE, &fluxes.leafOnCreation},
  debugFields->fluxDF[ind++] = (DebugField){"leafOnCreationFromWood", DEBUG_FIELD_DOUBLE, &fluxes.leafOnCreationFromWood},
  debugFields->fluxDF[ind++] = (DebugField){"leafOffLitter", DEBUG_FIELD_DOUBLE, &fluxes.leafOffLitter},
  debugFields->fluxDF[ind++] = (DebugField){"nVolatilization", DEBUG_FIELD_DOUBLE, &fluxes.nVolatilization},
  debugFields->fluxDF[ind++] = (DebugField){"nLeaching", DEBUG_FIELD_DOUBLE, &fluxes.nLeaching},
  debugFields->fluxDF[ind++] = (DebugField){"nOrgSoil", DEBUG_FIELD_DOUBLE, &fluxes.nOrgSoil},
  debugFields->fluxDF[ind++] = (DebugField){"nOrgLitter", DEBUG_FIELD_DOUBLE, &fluxes.nOrgLitter},
  debugFields->fluxDF[ind++] = (DebugField){"nMin", DEBUG_FIELD_DOUBLE, &fluxes.nMin},
  debugFields->fluxDF[ind++] = (DebugField){"nFixation", DEBUG_FIELD_DOUBLE, &fluxes.nFixation},
  debugFields->fluxDF[ind++] = (DebugField){"nUptake", DEBUG_FIELD_DOUBLE, &fluxes.nUptake},
  debugFields->fluxDF[ind++] = (DebugField){"leafOffNResorption", DEBUG_FIELD_DOUBLE, &fluxes.leafOffNResorption},
  debugFields->fluxDF[ind++] = (DebugField){"reductionNResorption", DEBUG_FIELD_DOUBLE, &fluxes.reductionNResorption},
  debugFields->fluxDF[ind++] = (DebugField){"eventLeafC", DEBUG_FIELD_DOUBLE, &fluxes.eventLeafC},
  debugFields->fluxDF[ind++] = (DebugField){"eventWoodC", DEBUG_FIELD_DOUBLE, &fluxes.eventWoodC},
  debugFields->fluxDF[ind++] = (DebugField){"eventAccountingC", DEBUG_FIELD_DOUBLE, &fluxes.eventAccountingC},
  debugFields->fluxDF[ind++] = (DebugField){"eventFineRootC", DEBUG_FIELD_DOUBLE, &fluxes.eventFineRootC},
  debugFields->fluxDF[ind++] = (DebugField){"eventCoarseRootC", DEBUG_FIELD_DOUBLE, &fluxes.eventCoarseRootC},
  debugFields->fluxDF[ind++] = (DebugField){"eventEvap", DEBUG_FIELD_DOUBLE, &fluxes.eventEvap},
  debugFields->fluxDF[ind++] = (DebugField){"eventSoilWater", DEBUG_FIELD_DOUBLE, &fluxes.eventSoilWater},
  debugFields->fluxDF[ind++] = (DebugField){"eventSoilC", DEBUG_FIELD_DOUBLE, &fluxes.eventSoilC},
  debugFields->fluxDF[ind++] = (DebugField){"eventLitterC", DEBUG_FIELD_DOUBLE, &fluxes.eventLitterC},
  debugFields->fluxDF[ind++] = (DebugField){"eventMinN", DEBUG_FIELD_DOUBLE, &fluxes.eventMinN},
  debugFields->fluxDF[ind++] = (DebugField){"eventSoilOrgN", DEBUG_FIELD_DOUBLE, &fluxes.eventSoilOrgN},
  debugFields->fluxDF[ind++] = (DebugField){"eventLitterN", DEBUG_FIELD_DOUBLE, &fluxes.eventLitterN},
  debugFields->fluxDF[ind++] = (DebugField){"eventInputC", DEBUG_FIELD_DOUBLE, &fluxes.eventInputC},
  debugFields->fluxDF[ind++] = (DebugField){"eventOutputC", DEBUG_FIELD_DOUBLE, &fluxes.eventOutputC},
  debugFields->fluxDF[ind++] = (DebugField){"eventInputN", DEBUG_FIELD_DOUBLE, &fluxes.eventInputN},
  debugFields->fluxDF[ind++] = (DebugField){"eventOutputN", DEBUG_FIELD_DOUBLE, &fluxes.eventOutputN},
  debugFields->fluxDF[ind++] = (DebugField){"eventLeafOnCreation", DEBUG_FIELD_DOUBLE, &fluxes.eventLeafOnCreation},
  debugFields->fluxDF[ind++] = (DebugField){"eventLeafOnCreationFromWood", DEBUG_FIELD_DOUBLE, &fluxes.eventLeafOnCreationFromWood},
  debugFields->fluxDF[ind++] = (DebugField){"eventLeafOffLitterC", DEBUG_FIELD_DOUBLE, &fluxes.eventLeafOffLitterC},
  debugFields->fluxDF[ind++] = (DebugField){"eventLeafOffLitterN", DEBUG_FIELD_DOUBLE, &fluxes.eventLeafOffLitterN},
  debugFields->fluxDF[ind++] = (DebugField){"eventLeafOffNResorption", DEBUG_FIELD_DOUBLE, &fluxes.eventLeafOffNResorption},
  debugFields->fluxDF[ind++] = (DebugField){"soilMethane", DEBUG_FIELD_DOUBLE, &fluxes.soilMethane},
  debugFields->fluxDF[ind++] = (DebugField){"litterMethane", DEBUG_FIELD_DOUBLE, &fluxes.litterMethane};
  if (ind != NUM_LOGGED_FLUX_FIELDS) {
    logInternalError("Debug log array size mismatch: fluxDF\n");
    exit(EXIT_CODE_INTERNAL_ERROR);
  }

  ind = 0;
  debugFields->trackerDF[ind++] = (DebugField){"gpp", DEBUG_FIELD_DOUBLE, &trackers.gpp},
  debugFields->trackerDF[ind++] = (DebugField){"rtot", DEBUG_FIELD_DOUBLE, &trackers.rtot},
  debugFields->trackerDF[ind++] = (DebugField){"ra", DEBUG_FIELD_DOUBLE, &trackers.ra},
  debugFields->trackerDF[ind++] = (DebugField){"rh", DEBUG_FIELD_DOUBLE, &trackers.rh},
  debugFields->trackerDF[ind++] = (DebugField){"rRoot", DEBUG_FIELD_DOUBLE, &trackers.rRoot},
  debugFields->trackerDF[ind++] = (DebugField){"rSoil", DEBUG_FIELD_DOUBLE, &trackers.rSoil},
  debugFields->trackerDF[ind++] = (DebugField){"rAboveground", DEBUG_FIELD_DOUBLE, &trackers.rAboveground},
  debugFields->trackerDF[ind++] = (DebugField){"npp", DEBUG_FIELD_DOUBLE, &trackers.npp},
  debugFields->trackerDF[ind++] = (DebugField){"nee", DEBUG_FIELD_DOUBLE, &trackers.nee},
  debugFields->trackerDF[ind++] = (DebugField){"woodCreation", DEBUG_FIELD_DOUBLE, &trackers.woodCreation},
  debugFields->trackerDF[ind++] = (DebugField){"gdd", DEBUG_FIELD_DOUBLE, &trackers.gdd},
  debugFields->trackerDF[ind++] = (DebugField){"evapotranspiration", DEBUG_FIELD_DOUBLE, &trackers.evapotranspiration},
  debugFields->trackerDF[ind++] = (DebugField){"soilWetnessFrac", DEBUG_FIELD_DOUBLE, &trackers.soilWetnessFrac},
  debugFields->trackerDF[ind++] = (DebugField){"yearlyGpp", DEBUG_FIELD_DOUBLE, &trackers.yearlyGpp},
  debugFields->trackerDF[ind++] = (DebugField){"yearlyRtot", DEBUG_FIELD_DOUBLE, &trackers.yearlyRtot},
  debugFields->trackerDF[ind++] = (DebugField){"yearlyRa", DEBUG_FIELD_DOUBLE, &trackers.yearlyRa},
  debugFields->trackerDF[ind++] = (DebugField){"yearlyRh", DEBUG_FIELD_DOUBLE, &trackers.yearlyRh},
  debugFields->trackerDF[ind++] = (DebugField){"yearlyNpp", DEBUG_FIELD_DOUBLE, &trackers.yearlyNpp},
  debugFields->trackerDF[ind++] = (DebugField){"yearlyNee", DEBUG_FIELD_DOUBLE, &trackers.yearlyNee},
  debugFields->trackerDF[ind++] = (DebugField){"yearlyLitter", DEBUG_FIELD_DOUBLE, &trackers.yearlyLitter},
  debugFields->trackerDF[ind++] = (DebugField){"totGpp", DEBUG_FIELD_DOUBLE, &trackers.totGpp},
  debugFields->trackerDF[ind++] = (DebugField){"totRtot", DEBUG_FIELD_DOUBLE, &trackers.totRtot},
  debugFields->trackerDF[ind++] = (DebugField){"totRa", DEBUG_FIELD_DOUBLE, &trackers.totRa},
  debugFields->trackerDF[ind++] = (DebugField){"totRh", DEBUG_FIELD_DOUBLE, &trackers.totRh},
  debugFields->trackerDF[ind++] = (DebugField){"totNpp", DEBUG_FIELD_DOUBLE, &trackers.totNpp},
  debugFields->trackerDF[ind++] = (DebugField){"totNee", DEBUG_FIELD_DOUBLE, &trackers.totNee},
  debugFields->trackerDF[ind++] = (DebugField){"lastYear", DEBUG_FIELD_INT, &trackers.lastYear},
  debugFields->trackerDF[ind++] = (DebugField){"methane", DEBUG_FIELD_DOUBLE, &trackers.methane},
  debugFields->trackerDF[ind++] = (DebugField){"n2o", DEBUG_FIELD_DOUBLE, &trackers.n2o},
  debugFields->trackerDF[ind++] = (DebugField){"nLeaching", DEBUG_FIELD_DOUBLE, &trackers.nLeaching},
  debugFields->trackerDF[ind++] = (DebugField){"nFixation", DEBUG_FIELD_DOUBLE, &trackers.nFixation},
  debugFields->trackerDF[ind++] = (DebugField){"nUptake", DEBUG_FIELD_DOUBLE, &trackers.nUptake};
  debugFields->trackerDF[ind++] = (DebugField){"meanNPP", DEBUG_FIELD_DOUBLE, &trackers.meanNPP};
  if (ind != NUM_LOGGED_TRACKER_FIELDS) {
    logInternalError("Debug log array size mismatch: trackerDF\n");
    exit(EXIT_CODE_INTERNAL_ERROR);
  }

  ind = 0;
  debugFields->phenoDF[ind++] = (DebugField){"didLeafGrowth", DEBUG_FIELD_INT, &phenologyTrackers.didLeafGrowth},
  debugFields->phenoDF[ind++] = (DebugField){"didLeafFall", DEBUG_FIELD_INT, &phenologyTrackers.didLeafFall},
  debugFields->phenoDF[ind++] = (DebugField){"lastYear", DEBUG_FIELD_INT, &phenologyTrackers.lastYear};
  if (ind != NUM_LOGGED_PHEN_TRACKER_FIELDS) {
    logInternalError("Debug log array size mismatch: phenoDF\n");
    exit(EXIT_CODE_INTERNAL_ERROR);
  }

  ind = 0;
  debugFields->survivalDF[ind++] = (DebugField){"isAlive", DEBUG_FIELD_INT, &plantSurvivalTracker.isAlive};
  if (ind != NUM_LOGGED_SURVIVAL_FIELDS) {
    logInternalError("Debug log array size mismatch: survivalDF\n");
    exit(EXIT_CODE_INTERNAL_ERROR);
  }

  ind = 0;
  debugFields->eventDF[ind++] = (DebugField){"d_till_mod", DEBUG_FIELD_DOUBLE, &eventTrackers.d_till_mod};
  debugFields->eventDF[ind++] = (DebugField){"ht.totalFracRemoved", DEBUG_FIELD_DOUBLE, &eventTrackers.harvestTrackers.totalFracRemoved};
  debugFields->eventDF[ind++] = (DebugField){"ht.totalFracTransferred", DEBUG_FIELD_DOUBLE, &eventTrackers.harvestTrackers.totalFracTransferred};
  debugFields->eventDF[ind++] = (DebugField){"ht.totalFracRemovedAbove", DEBUG_FIELD_DOUBLE, &eventTrackers.harvestTrackers.totalFracRemovedAbove};
  debugFields->eventDF[ind++] = (DebugField){"ht.totalFracRemovedBelow", DEBUG_FIELD_DOUBLE, &eventTrackers.harvestTrackers.totalFracRemovedBelow};
  debugFields->eventDF[ind++] = (DebugField){"ht.totalFracTransferredAbove", DEBUG_FIELD_DOUBLE, &eventTrackers.harvestTrackers.totalFracTransferredAbove};
  debugFields->eventDF[ind++] = (DebugField){"ht.totalFracTransferredBelow", DEBUG_FIELD_DOUBLE, &eventTrackers.harvestTrackers.totalFracTransferredBelow};
  if (ind != NUM_LOGGED_EVENT_TRACKER_FIELDS) {
    logInternalError("Debug log array size mismatch: eventDF\n");
    exit(EXIT_CODE_INTERNAL_ERROR);
  }
  // clang-format on
}

static FILE *openDebugLogFile(const char *debugLogPrefix, const char *suffix) {
  char filename[FILENAME_MAXLEN];

  const int written =
      snprintf(filename, sizeof(filename), "%s%s", debugLogPrefix, suffix);
  if (written < 0 || (size_t)written >= sizeof(filename)) {
    logError("debug-log prefix '%s' is too long\n", debugLogPrefix);
    exit(EXIT_CODE_BAD_PARAMETER_VALUE);
  }

  return openFile(filename, "w");
}

static void outputDebugFieldHeader(FILE *out, const char *prefix,
                                   const DebugField *fields, size_t numFields,
                                   int fullLine) {
  if (fullLine) {
    fprintf(out, "year day time");
  }
  for (size_t ind = 0; ind < numFields; ++ind) {
    fprintf(out, " %s%s", prefix, fields[ind].name);
  }
  if (fullLine) {
    fprintf(out, "\n");
  }
}

static void outputDebugFieldValues(FILE *out, int year, int day, double time,
                                   const DebugField *fields, size_t numFields,
                                   int fullLine) {
  if (fullLine) {
    fprintf(out, "%4d %3d %5.2f", year, day, time);
  }
  for (size_t ind = 0; ind < numFields; ++ind) {
    if (fields[ind].type == DEBUG_FIELD_INT) {
      fprintf(out, " %d", *((const int *)fields[ind].value));
    } else {
      fprintf(out, " %.15g", *((const double *)fields[ind].value));
    }
  }
  if (fullLine) {
    fprintf(out, "\n");
  }
}

void initDebugLogFiles(DebugLogFiles *debugLogFiles) {
  if (debugLogFiles == NULL) {
    return;
  }

  debugLogFiles->envi = NULL;
  debugLogFiles->fluxes = NULL;
  debugLogFiles->trackers = NULL;
}

void openDebugLogFiles(DebugLogFiles *debugLogFiles,
                       const char *debugLogPrefix) {
  if ((debugLogFiles == NULL) || (debugLogPrefix == NULL) ||
      (strlen(debugLogPrefix) == 0)) {
    return;
  }

  debugLogFiles->envi = openDebugLogFile(debugLogPrefix, "_envi.log");
  debugLogFiles->fluxes = openDebugLogFile(debugLogPrefix, "_fluxes.log");
  debugLogFiles->trackers = openDebugLogFile(debugLogPrefix, "_trackers.log");
}

void closeDebugLogFiles(DebugLogFiles *debugLogFiles) {
  if (debugLogFiles == NULL) {
    return;
  }

  if (debugLogFiles->envi != NULL) {
    fclose(debugLogFiles->envi);
  }
  if (debugLogFiles->fluxes != NULL) {
    fclose(debugLogFiles->fluxes);
  }
  if (debugLogFiles->trackers != NULL) {
    fclose(debugLogFiles->trackers);
  }
}

void freeDebugArrays(void) {
  if (debugFields != NULL) {
    free(debugFields);
  }
}

void outputDebugHeaders(DebugLogFiles *debugLogFiles) {
  if (debugLogFiles == NULL || debugFields == NULL) {
    return;
  }

  if (debugLogFiles->envi != NULL) {
    outputDebugFieldHeader(debugLogFiles->envi, "", debugFields->enviDF,
                           NUM_LOGGED_ENVI_FIELDS, 1);
  }
  if (debugLogFiles->fluxes != NULL) {
    outputDebugFieldHeader(debugLogFiles->fluxes, "", debugFields->fluxDF,
                           NUM_LOGGED_FLUX_FIELDS, 1);
  }
  if (debugLogFiles->trackers != NULL) {
    fprintf(debugLogFiles->trackers, "year day time");
    outputDebugFieldHeader(debugLogFiles->trackers, "t.",
                           debugFields->trackerDF, NUM_LOGGED_TRACKER_FIELDS,
                           0);
    outputDebugFieldHeader(debugLogFiles->trackers, "pt.", debugFields->phenoDF,
                           NUM_LOGGED_PHEN_TRACKER_FIELDS, 0);
    outputDebugFieldHeader(debugLogFiles->trackers, "s.",
                           debugFields->survivalDF, NUM_LOGGED_SURVIVAL_FIELDS,
                           0);
    outputDebugFieldHeader(debugLogFiles->trackers, "et.", debugFields->eventDF,
                           NUM_LOGGED_EVENT_TRACKER_FIELDS, 0);
    fprintf(debugLogFiles->trackers, "\n");
  }
}

void outputDebugState(DebugLogFiles *debugLogFiles, int year, int day,
                      double time) {
  if (debugLogFiles == NULL || debugFields == NULL) {
    return;
  }

  if (debugLogFiles->envi != NULL) {
    outputDebugFieldValues(debugLogFiles->envi, year, day, time,
                           debugFields->enviDF, NUM_LOGGED_ENVI_FIELDS, 1);
  }
  if (debugLogFiles->fluxes != NULL) {
    outputDebugFieldValues(debugLogFiles->fluxes, year, day, time,
                           debugFields->fluxDF, NUM_LOGGED_FLUX_FIELDS, 1);
  }
  if (debugLogFiles->trackers != NULL) {
    fprintf(debugLogFiles->trackers, "%4d %3d %5.2f", year, day, time);
    outputDebugFieldValues(debugLogFiles->trackers, year, day, time,
                           debugFields->trackerDF, NUM_LOGGED_TRACKER_FIELDS,
                           0);
    outputDebugFieldValues(debugLogFiles->trackers, year, day, time,
                           debugFields->phenoDF, NUM_LOGGED_PHEN_TRACKER_FIELDS,
                           0);
    outputDebugFieldValues(debugLogFiles->trackers, year, day, time,
                           debugFields->survivalDF, NUM_LOGGED_SURVIVAL_FIELDS,
                           0);
    outputDebugFieldValues(debugLogFiles->trackers, year, day, time,
                           debugFields->eventDF,
                           NUM_LOGGED_EVENT_TRACKER_FIELDS, 0);
    fprintf(debugLogFiles->trackers, "\n");
  }
}

/* debug_log: structures and functions for SIPNET debug state logging
 */

#include <stdio.h>
#include <string.h>

#include "debug_log.h"

#include "common/context.h"
#include "common/util.h"
#include "state.h"

typedef enum DebugFieldType {
  DEBUG_FIELD_INT = 0,
  DEBUG_FIELD_DOUBLE = 1
} DebugFieldType;

typedef struct DebugField {
  const char *name;
  DebugFieldType type;
  const void *value;
} DebugField;

static const DebugField ENVI_DEBUG_FIELDS[] = {
    {"plantWoodC", DEBUG_FIELD_DOUBLE, &envi.plantWoodC},
    {"plantLeafC", DEBUG_FIELD_DOUBLE, &envi.plantLeafC},
    {"soilC", DEBUG_FIELD_DOUBLE, &envi.soilC},
    {"soilWater", DEBUG_FIELD_DOUBLE, &envi.soilWater},
    {"litterC", DEBUG_FIELD_DOUBLE, &envi.litterC},
    {"snow", DEBUG_FIELD_DOUBLE, &envi.snow},
    {"coarseRootC", DEBUG_FIELD_DOUBLE, &envi.coarseRootC},
    {"fineRootC", DEBUG_FIELD_DOUBLE, &envi.fineRootC},
    {"minN", DEBUG_FIELD_DOUBLE, &envi.minN},
    {"soilOrgN", DEBUG_FIELD_DOUBLE, &envi.soilOrgN},
    {"litterN", DEBUG_FIELD_DOUBLE, &envi.litterN},
    {"plantStorageN", DEBUG_FIELD_DOUBLE, &envi.plantStorageN},
    {"plantWoodCStorageDelta", DEBUG_FIELD_DOUBLE,
     &envi.plantWoodCStorageDelta}};

static const DebugField FLUXES_DEBUG_FIELDS[] = {
    {"photosynthesis", DEBUG_FIELD_DOUBLE, &fluxes.photosynthesis},
    {"leafLitter", DEBUG_FIELD_DOUBLE, &fluxes.leafLitter},
    {"woodLitter", DEBUG_FIELD_DOUBLE, &fluxes.woodLitter},
    {"rVeg", DEBUG_FIELD_DOUBLE, &fluxes.rVeg},
    {"rSoil", DEBUG_FIELD_DOUBLE, &fluxes.rSoil},
    {"rain", DEBUG_FIELD_DOUBLE, &fluxes.rain},
    {"transpiration", DEBUG_FIELD_DOUBLE, &fluxes.transpiration},
    {"drainage", DEBUG_FIELD_DOUBLE, &fluxes.drainage},
    {"litterToSoil", DEBUG_FIELD_DOUBLE, &fluxes.litterToSoil},
    {"rLitter", DEBUG_FIELD_DOUBLE, &fluxes.rLitter},
    {"snowFall", DEBUG_FIELD_DOUBLE, &fluxes.snowFall},
    {"snowMelt", DEBUG_FIELD_DOUBLE, &fluxes.snowMelt},
    {"sublimation", DEBUG_FIELD_DOUBLE, &fluxes.sublimation},
    {"immedEvap", DEBUG_FIELD_DOUBLE, &fluxes.immedEvap},
    {"fastFlow", DEBUG_FIELD_DOUBLE, &fluxes.fastFlow},
    {"evaporation", DEBUG_FIELD_DOUBLE, &fluxes.evaporation},
    {"fineRootLoss", DEBUG_FIELD_DOUBLE, &fluxes.fineRootLoss},
    {"coarseRootLoss", DEBUG_FIELD_DOUBLE, &fluxes.coarseRootLoss},
    {"fineRootCreation", DEBUG_FIELD_DOUBLE, &fluxes.fineRootCreation},
    {"coarseRootCreation", DEBUG_FIELD_DOUBLE, &fluxes.coarseRootCreation},
    {"rCoarseRoot", DEBUG_FIELD_DOUBLE, &fluxes.rCoarseRoot},
    {"rFineRoot", DEBUG_FIELD_DOUBLE, &fluxes.rFineRoot},
    {"leafCreation", DEBUG_FIELD_DOUBLE, &fluxes.leafCreation},
    {"woodCreation", DEBUG_FIELD_DOUBLE, &fluxes.woodCreation},
    {"leafOnCreation", DEBUG_FIELD_DOUBLE, &fluxes.leafOnCreation},
    {"leafOnCreationFromWood", DEBUG_FIELD_DOUBLE,
     &fluxes.leafOnCreationFromWood},
    {"nVolatilization", DEBUG_FIELD_DOUBLE, &fluxes.nVolatilization},
    {"nLeaching", DEBUG_FIELD_DOUBLE, &fluxes.nLeaching},
    {"nOrgSoil", DEBUG_FIELD_DOUBLE, &fluxes.nOrgSoil},
    {"nOrgLitter", DEBUG_FIELD_DOUBLE, &fluxes.nOrgLitter},
    {"nMin", DEBUG_FIELD_DOUBLE, &fluxes.nMin},
    {"nFixation", DEBUG_FIELD_DOUBLE, &fluxes.nFixation},
    {"nUptake", DEBUG_FIELD_DOUBLE, &fluxes.nUptake},
    {"leafOffNResorption", DEBUG_FIELD_DOUBLE, &fluxes.leafOffNResorption},
    {"eventLeafC", DEBUG_FIELD_DOUBLE, &fluxes.eventLeafC},
    {"eventWoodC", DEBUG_FIELD_DOUBLE, &fluxes.eventWoodC},
    {"eventFineRootC", DEBUG_FIELD_DOUBLE, &fluxes.eventFineRootC},
    {"eventCoarseRootC", DEBUG_FIELD_DOUBLE, &fluxes.eventCoarseRootC},
    {"eventEvap", DEBUG_FIELD_DOUBLE, &fluxes.eventEvap},
    {"eventSoilWater", DEBUG_FIELD_DOUBLE, &fluxes.eventSoilWater},
    {"eventSoilC", DEBUG_FIELD_DOUBLE, &fluxes.eventSoilC},
    {"eventLitterC", DEBUG_FIELD_DOUBLE, &fluxes.eventLitterC},
    {"eventMinN", DEBUG_FIELD_DOUBLE, &fluxes.eventMinN},
    {"eventSoilOrgN", DEBUG_FIELD_DOUBLE, &fluxes.eventSoilOrgN},
    {"eventLitterN", DEBUG_FIELD_DOUBLE, &fluxes.eventLitterN},
    {"eventInputC", DEBUG_FIELD_DOUBLE, &fluxes.eventInputC},
    {"eventOutputC", DEBUG_FIELD_DOUBLE, &fluxes.eventOutputC},
    {"eventInputN", DEBUG_FIELD_DOUBLE, &fluxes.eventInputN},
    {"eventOutputN", DEBUG_FIELD_DOUBLE, &fluxes.eventOutputN},
    {"eventLeafOnCreation", DEBUG_FIELD_DOUBLE, &fluxes.eventLeafOnCreation},
    {"eventLeafOnCreationFromWood", DEBUG_FIELD_DOUBLE,
     &fluxes.eventLeafOnCreationFromWood},
    {"eventLeafOffLitter", DEBUG_FIELD_DOUBLE, &fluxes.eventLeafOffLitter},
    {"eventLeafOffNResorption", DEBUG_FIELD_DOUBLE,
     &fluxes.eventLeafOffNResorption},
    {"soilMethane", DEBUG_FIELD_DOUBLE, &fluxes.soilMethane},
    {"litterMethane", DEBUG_FIELD_DOUBLE, &fluxes.litterMethane}};

static const DebugField TRACKERS_DEBUG_FIELDS[] = {
    {"gpp", DEBUG_FIELD_DOUBLE, &trackers.gpp},
    {"rtot", DEBUG_FIELD_DOUBLE, &trackers.rtot},
    {"ra", DEBUG_FIELD_DOUBLE, &trackers.ra},
    {"rh", DEBUG_FIELD_DOUBLE, &trackers.rh},
    {"rRoot", DEBUG_FIELD_DOUBLE, &trackers.rRoot},
    {"rSoil", DEBUG_FIELD_DOUBLE, &trackers.rSoil},
    {"rAboveground", DEBUG_FIELD_DOUBLE, &trackers.rAboveground},
    {"npp", DEBUG_FIELD_DOUBLE, &trackers.npp},
    {"nee", DEBUG_FIELD_DOUBLE, &trackers.nee},
    {"woodCreation", DEBUG_FIELD_DOUBLE, &trackers.woodCreation},
    {"gdd", DEBUG_FIELD_DOUBLE, &trackers.gdd},
    {"evapotranspiration", DEBUG_FIELD_DOUBLE, &trackers.evapotranspiration},
    {"soilWetnessFrac", DEBUG_FIELD_DOUBLE, &trackers.soilWetnessFrac},
    {"yearlyGpp", DEBUG_FIELD_DOUBLE, &trackers.yearlyGpp},
    {"yearlyRtot", DEBUG_FIELD_DOUBLE, &trackers.yearlyRtot},
    {"yearlyRa", DEBUG_FIELD_DOUBLE, &trackers.yearlyRa},
    {"yearlyRh", DEBUG_FIELD_DOUBLE, &trackers.yearlyRh},
    {"yearlyNpp", DEBUG_FIELD_DOUBLE, &trackers.yearlyNpp},
    {"yearlyNee", DEBUG_FIELD_DOUBLE, &trackers.yearlyNee},
    {"yearlyLitter", DEBUG_FIELD_DOUBLE, &trackers.yearlyLitter},
    {"totGpp", DEBUG_FIELD_DOUBLE, &trackers.totGpp},
    {"totRtot", DEBUG_FIELD_DOUBLE, &trackers.totRtot},
    {"totRa", DEBUG_FIELD_DOUBLE, &trackers.totRa},
    {"totRh", DEBUG_FIELD_DOUBLE, &trackers.totRh},
    {"totNpp", DEBUG_FIELD_DOUBLE, &trackers.totNpp},
    {"totNee", DEBUG_FIELD_DOUBLE, &trackers.totNee},
    {"lastYear", DEBUG_FIELD_INT, &trackers.lastYear},
    {"methane", DEBUG_FIELD_DOUBLE, &trackers.methane},
    {"n2o", DEBUG_FIELD_DOUBLE, &trackers.n2o},
    {"nLeaching", DEBUG_FIELD_DOUBLE, &trackers.nLeaching},
    {"nFixation", DEBUG_FIELD_DOUBLE, &trackers.nFixation},
    {"nUptake", DEBUG_FIELD_DOUBLE, &trackers.nUptake}};

static const DebugField PHENOLOGY_DEBUG_FIELDS[] = {
    {"didLeafGrowth", DEBUG_FIELD_INT, &phenologyTrackers.didLeafGrowth},
    {"didLeafFall", DEBUG_FIELD_INT, &phenologyTrackers.didLeafFall},
    {"lastYear", DEBUG_FIELD_INT, &phenologyTrackers.lastYear}};

static FILE *openDebugOutputFile(const char *debugOutputPrefix,
                                 const char *suffix) {
  char filename[FILENAME_MAXLEN];

  strcpy(filename, debugOutputPrefix);
  strcat(filename, suffix);

  return openFile(filename, "w");
}

static void outputDebugFieldHeader(FILE *out, const char *prefix,
                                   const DebugField *fields,
                                   size_t numFields) {
  fprintf(out, "year day time");
  for (size_t ind = 0; ind < numFields; ++ind) {
    fprintf(out, " %s%s", prefix, fields[ind].name);
  }
  fprintf(out, "\n");
}

static void outputDebugFieldValues(FILE *out, int year, int day, double time,
                                   const DebugField *fields,
                                   size_t numFields) {
  fprintf(out, "%4d %3d %5.2f", year, day, time);
  for (size_t ind = 0; ind < numFields; ++ind) {
    if (fields[ind].type == DEBUG_FIELD_INT) {
      fprintf(out, " %d", *((const int *)fields[ind].value));
    } else {
      fprintf(out, " %.15g", *((const double *)fields[ind].value));
    }
  }
  fprintf(out, "\n");
}

void initDebugOutputFiles(DebugOutputFiles *debugOutputFiles) {
  if (debugOutputFiles == NULL) {
    return;
  }

  debugOutputFiles->envi = NULL;
  debugOutputFiles->fluxes = NULL;
  debugOutputFiles->trackers = NULL;
}

void openDebugOutputFiles(DebugOutputFiles *debugOutputFiles,
                          const char *debugOutputPrefix) {
  if ((debugOutputFiles == NULL) || (debugOutputPrefix == NULL) ||
      (strlen(debugOutputPrefix) == 0)) {
    return;
  }

  debugOutputFiles->envi =
      openDebugOutputFile(debugOutputPrefix, "_envi.log");
  debugOutputFiles->fluxes =
      openDebugOutputFile(debugOutputPrefix, "_fluxes.log");
  debugOutputFiles->trackers =
      openDebugOutputFile(debugOutputPrefix, "_trackers.log");
}

void closeDebugOutputFiles(DebugOutputFiles *debugOutputFiles) {
  if (debugOutputFiles == NULL) {
    return;
  }

  if (debugOutputFiles->envi != NULL) {
    fclose(debugOutputFiles->envi);
  }
  if (debugOutputFiles->fluxes != NULL) {
    fclose(debugOutputFiles->fluxes);
  }
  if (debugOutputFiles->trackers != NULL) {
    fclose(debugOutputFiles->trackers);
  }
}

void outputDebugHeaders(DebugOutputFiles *debugOutputFiles) {
  if (debugOutputFiles == NULL) {
    return;
  }

  if (debugOutputFiles->envi != NULL) {
    outputDebugFieldHeader(debugOutputFiles->envi, "", ENVI_DEBUG_FIELDS,
                           sizeof(ENVI_DEBUG_FIELDS) / sizeof(DebugField));
  }
  if (debugOutputFiles->fluxes != NULL) {
    outputDebugFieldHeader(debugOutputFiles->fluxes, "", FLUXES_DEBUG_FIELDS,
                           sizeof(FLUXES_DEBUG_FIELDS) / sizeof(DebugField));
  }
  if (debugOutputFiles->trackers != NULL) {
    fprintf(debugOutputFiles->trackers, "year day time");
    for (size_t ind = 0;
         ind < sizeof(TRACKERS_DEBUG_FIELDS) / sizeof(DebugField); ++ind) {
      fprintf(debugOutputFiles->trackers, " t.%s",
              TRACKERS_DEBUG_FIELDS[ind].name);
    }
    for (size_t ind = 0;
         ind < sizeof(PHENOLOGY_DEBUG_FIELDS) / sizeof(DebugField); ++ind) {
      fprintf(debugOutputFiles->trackers, " pt.%s",
              PHENOLOGY_DEBUG_FIELDS[ind].name);
    }
    fprintf(debugOutputFiles->trackers, "\n");
  }
}

void outputDebugState(DebugOutputFiles *debugOutputFiles, int year, int day,
                      double time) {
  if (debugOutputFiles == NULL) {
    return;
  }

  if (debugOutputFiles->envi != NULL) {
    outputDebugFieldValues(debugOutputFiles->envi, year, day, time,
                           ENVI_DEBUG_FIELDS,
                           sizeof(ENVI_DEBUG_FIELDS) / sizeof(DebugField));
  }
  if (debugOutputFiles->fluxes != NULL) {
    outputDebugFieldValues(debugOutputFiles->fluxes, year, day, time,
                           FLUXES_DEBUG_FIELDS,
                           sizeof(FLUXES_DEBUG_FIELDS) / sizeof(DebugField));
  }
  if (debugOutputFiles->trackers != NULL) {
    fprintf(debugOutputFiles->trackers, "%4d %3d %5.2f", year, day, time);
    for (size_t ind = 0;
         ind < sizeof(TRACKERS_DEBUG_FIELDS) / sizeof(DebugField); ++ind) {
      if (TRACKERS_DEBUG_FIELDS[ind].type == DEBUG_FIELD_INT) {
        fprintf(debugOutputFiles->trackers, " %d",
                *((const int *)TRACKERS_DEBUG_FIELDS[ind].value));
      } else {
        fprintf(debugOutputFiles->trackers, " %.15g",
                *((const double *)TRACKERS_DEBUG_FIELDS[ind].value));
      }
    }
    for (size_t ind = 0;
         ind < sizeof(PHENOLOGY_DEBUG_FIELDS) / sizeof(DebugField); ++ind) {
      fprintf(debugOutputFiles->trackers, " %d",
              *((const int *)PHENOLOGY_DEBUG_FIELDS[ind].value));
    }
    fprintf(debugOutputFiles->trackers, "\n");
  }
}

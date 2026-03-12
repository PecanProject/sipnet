#include "restart.h"

#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "common/context.h"
#include "common/exitCodes.h"
#include "common/logging.h"
#include "common/util.h"
#include "events.h"
#include "version.h"

#define RESTART_MAGIC "SIPNET_RESTART"
#define RESTART_SCHEMA_VERSION "1.0"
#define RESTART_FLOAT_EPSILON 1e-8

/*
 * When the serialized restart contract changes, update:
 * - Number of elements of changed payload struct (#defs immediately below)
 * - RESTART_SCHEMA_LAYOUT_* constants and related runtime checks
 * - restart read/write logic, docs, and restart tests
 * - RESTART_SCHEMA_VERSION
 */

#define MODEL_VERSION_BUFFER_SIZE 32
#define BUILD_INFO_BUFFER_SIZE 96
#define FIELD_SEEN (-1)
#define FIELD_INVALID (-2)
#define NUM_META_FIELDS 4
#define NUM_SCHEMA_FIELDS 4
#define NUM_MEAN_META_FIELDS 5
#define NUM_ENVI_FIELDS 12
#define NUM_TRACKER_FIELDS 32
#define NUM_PHENOLOGY_TRACKERS_FIELDS 3
#define NUM_EVENT_TRACKERS_FIELDS 1

// This one shouldn't change
#define NUM_END_FIELDS 1

#define RESTART_SCHEMA_LAYOUT_ENVI_SIZE (8 * NUM_ENVI_FIELDS)
#define RESTART_SCHEMA_LAYOUT_TRACKERS_SIZE (8 * NUM_TRACKER_FIELDS)
#define RESTART_SCHEMA_LAYOUT_PHENOLOGY_TRACKERS_SIZE                          \
  (4 * NUM_PHENOLOGY_TRACKERS_FIELDS)
#define RESTART_SCHEMA_LAYOUT_EVENT_TRACKERS_SIZE                              \
  (8 * NUM_EVENT_TRACKERS_FIELDS)

_Static_assert(sizeof(Envi) == RESTART_SCHEMA_LAYOUT_ENVI_SIZE,
               "Restart schema 1.0 drift: Envi changed; bump restart schema "
               "version and update schema_layout.* checks");
_Static_assert(sizeof(Trackers) == RESTART_SCHEMA_LAYOUT_TRACKERS_SIZE,
               "Restart schema 1.0 drift: serialized trackers payload changed; "
               "bump restart schema version and update schema_layout.* checks");
_Static_assert(
    sizeof(PhenologyTrackers) == RESTART_SCHEMA_LAYOUT_PHENOLOGY_TRACKERS_SIZE,
    "Restart schema 1.0 drift: PhenologyTrackers changed; bump restart "
    "schema version and update schema_layout.* checks");
_Static_assert(sizeof(EventTrackers) ==
                   RESTART_SCHEMA_LAYOUT_EVENT_TRACKERS_SIZE,
               "Restart schema 1.0 drift: EventTrackers changed; bump restart "
               "schema version and update schema_layout.* checks");

#define NUM_CLIMATE_SIGNATURE_FIELDS 4
typedef struct RestartClimateSignature {
  int year;
  int day;
  double time;
  double length;
} RestartClimateSignature;
static RestartClimateSignature boundaryClimate;

// NUM_CONTEXT_MODEL_FLAGS is defined in context.h, as that is the authoritative
// source
typedef struct RestartContextModelFlags {
  // Context flags that alter model runtime behavior
  int events;
  int gdd;
  int growthResp;
  int leafWater;
  int litterPool;
  int snow;
  int soilPhenol;
  int waterHResp;
  int nitrogenCycle;
  int anaerobic;
} RestartContextModelFlags;
static RestartContextModelFlags modelFlags;

_Static_assert(sizeof(RestartContextModelFlags) == NUM_CONTEXT_MODEL_FLAGS * 4,
               "Restart schema 1.0 drift: Model flags changed; bump restart "
               "schema version and update schema_layout.* checks");

typedef enum StateFieldType {
  FT_LONGLONG = 0,
  FT_INT = 1,
  FT_DOUBLE = 2,
  FT_CHAR = 3,
  FT_SPECIAL = 4,  // writes 'seen' value as INT
  FT_INVALID = 5
} FieldType;

typedef struct StateField_s {
  char key[128];
  enum StateFieldType type;
  void *value;
  // equals FIELD_SEEN if this feel has been read/written; also has other
  // sentinel purposes for FT_CHAR and FT_SPECIAL
  int seen;
} StateField;

// **********
// Tracking vars and storage for non-sipnet values
// Make sure to handle the ones that modify SIPNET state in both read and write
static long long processedStepCount = 0;
static long long checkpointUTCEpoch = 0;
static const ClimateNode *lastProcessedClimateStep = NULL;
static char modelVersion[MODEL_VERSION_BUFFER_SIZE] = {0};
static char buildInfo[BUILD_INFO_BUFFER_SIZE] = {0};
static int endRestart = 0;

typedef struct RestartState_s {
  // Note: meanNPP value and weight arrays are still handled separately
  StateField metaPF[NUM_META_FIELDS + 1];
  StateField schemaPF[NUM_SCHEMA_FIELDS + 1];
  StateField flagsPF[NUM_CONTEXT_MODEL_FLAGS + 1];
  StateField boundaryPF[NUM_CLIMATE_SIGNATURE_FIELDS + 1];
  StateField nppPF[NUM_MEAN_META_FIELDS + 1];
  StateField enviPF[NUM_ENVI_FIELDS + 1];
  StateField trackersPF[NUM_TRACKER_FIELDS + 1];
  StateField phenologyPF[NUM_PHENOLOGY_TRACKERS_FIELDS + 1];
  StateField eventPF[NUM_EVENT_TRACKERS_FIELDS + 1];
  StateField endPF[1];  // Should only ever be exactly one here
} RestartState;

void initResetState(RestartState *state, MeanTracker *npp) {
  // clang-format off
  // NOLINTBEGIN
  state->metaPF[0] = (StateField){"meta_info.model_version",        FT_CHAR,      modelVersion,       MODEL_VERSION_BUFFER_SIZE};
  state->metaPF[1] = (StateField){"meta_info.build_info",           FT_CHAR,      buildInfo,          BUILD_INFO_BUFFER_SIZE};
  state->metaPF[2] = (StateField){"meta_info.checkpoint_utc_epoch", FT_LONGLONG, &checkpointUTCEpoch, 0};
  state->metaPF[3] = (StateField){"meta_info.processed_steps",      FT_LONGLONG, &processedStepCount, 0};
  state->metaPF[4] = (StateField){"meta.info.invalid",              FT_INVALID,   NULL,               FIELD_INVALID};

  state->schemaPF[0] = (StateField){"schema_layout.envi_size",               FT_SPECIAL, 0, RESTART_SCHEMA_LAYOUT_ENVI_SIZE};
  state->schemaPF[1] = (StateField){"schema_layout.trackers_size",           FT_SPECIAL, 0, RESTART_SCHEMA_LAYOUT_TRACKERS_SIZE};
  state->schemaPF[2] = (StateField){"schema_layout.phenology_trackers_size", FT_SPECIAL, 0, RESTART_SCHEMA_LAYOUT_PHENOLOGY_TRACKERS_SIZE};
  state->schemaPF[3] = (StateField){"schema_layout.event_trackers_size",     FT_SPECIAL, 0, RESTART_SCHEMA_LAYOUT_EVENT_TRACKERS_SIZE};
  state->schemaPF[4] = (StateField){"schema_layout.invalid",                 FT_INVALID, NULL, FIELD_INVALID};

  state->flagsPF[0] = (StateField){"flags.events",        FT_INT, &modelFlags.events,        0};
  state->flagsPF[1] = (StateField){"flags.gdd",           FT_INT, &modelFlags.gdd,           0};
  state->flagsPF[2] = (StateField){"flags.growthResp",    FT_INT, &modelFlags.growthResp,    0};
  state->flagsPF[3] = (StateField){"flags.leafWater",     FT_INT, &modelFlags.leafWater,     0};
  state->flagsPF[4] = (StateField){"flags.litterPool",    FT_INT, &modelFlags.litterPool,    0};
  state->flagsPF[5] = (StateField){"flags.snow",          FT_INT, &modelFlags.snow,          0};
  state->flagsPF[6] = (StateField){"flags.soilPhenol",    FT_INT, &modelFlags.soilPhenol,    0};
  state->flagsPF[7] = (StateField){"flags.waterHResp",    FT_INT, &modelFlags.waterHResp,    0};
  state->flagsPF[8] = (StateField){"flags.nitrogenCycle", FT_INT, &modelFlags.nitrogenCycle, 0};
  state->flagsPF[9] = (StateField){"flags.anaerobic",     FT_INT, &modelFlags.anaerobic,     0};
  state->flagsPF[10] = (StateField){"flags.invalid",      FT_INVALID, NULL, FIELD_INVALID};

  state->boundaryPF[0] = (StateField){"boundary.year",    FT_INT,     &boundaryClimate.year,   0};
  state->boundaryPF[1] = (StateField){"boundary.day",     FT_INT,     &boundaryClimate.day,    0};
  state->boundaryPF[2] = (StateField){"boundary.time",    FT_DOUBLE,  &boundaryClimate.time,   0};
  state->boundaryPF[3] = (StateField){"boundary.length",  FT_DOUBLE,  &boundaryClimate.length, 0};
  state->boundaryPF[4] = (StateField){"boundary.invalid", FT_INVALID, NULL, FIELD_INVALID};

  state->nppPF[0] = (StateField){"mean.npp.length",    FT_INT,     &npp->length,    0};
  state->nppPF[1] = (StateField){"mean.npp.totWeight", FT_DOUBLE,  &npp->totWeight, 0};
  state->nppPF[2] = (StateField){"mean.npp.start",     FT_INT,     &npp->start,     0};
  state->nppPF[3] = (StateField){"mean.npp.last",      FT_INT,     &npp->last,      0};
  state->nppPF[4] = (StateField){"mean.npp.sum",       FT_DOUBLE,  &npp->sum,       0};
  state->nppPF[5] = (StateField){"mean.npp.invalid",   FT_INVALID, NULL, FIELD_INVALID};

  state->enviPF[0] = (StateField){"envi.plantWoodC",              FT_DOUBLE, &envi.plantWoodC,             0};
  state->enviPF[1] = (StateField){"envi.plantLeafC",              FT_DOUBLE, &envi.plantLeafC,             0};
  state->enviPF[2] = (StateField){"envi.soilC",                   FT_DOUBLE, &envi.soilC,                  0};
  state->enviPF[3] = (StateField){"envi.soilWater",               FT_DOUBLE, &envi.soilWater,              0};
  state->enviPF[4] = (StateField){"envi.litterC",                 FT_DOUBLE, &envi.litterC,                0};
  state->enviPF[5] = (StateField){"envi.snow",                    FT_DOUBLE, &envi.snow,                   0};
  state->enviPF[6] = (StateField){"envi.coarseRootC",             FT_DOUBLE, &envi.coarseRootC,            0};
  state->enviPF[7] = (StateField){"envi.fineRootC",               FT_DOUBLE, &envi.fineRootC,              0};
  state->enviPF[8] = (StateField){"envi.minN",                    FT_DOUBLE, &envi.minN,                   0};
  state->enviPF[9] = (StateField){"envi.soilOrgN",                FT_DOUBLE, &envi.soilOrgN,               0};
  state->enviPF[10] = (StateField){"envi.litterN",                FT_DOUBLE, &envi.litterN,                0};
  state->enviPF[11] = (StateField){"envi.plantWoodCStorageDelta", FT_DOUBLE, &envi.plantWoodCStorageDelta, 0};
  state->enviPF[12] = (StateField){"envi.invalid",                FT_INVALID, NULL, FIELD_INVALID};

  state->trackersPF[0] = (StateField){"trackers.gpp",                 FT_DOUBLE, &trackers.gpp,                0};
  state->trackersPF[1] = (StateField){"trackers.rtot",                FT_DOUBLE, &trackers.rtot,               0};
  state->trackersPF[2] = (StateField){"trackers.ra",                  FT_DOUBLE, &trackers.ra,                 0};
  state->trackersPF[3] = (StateField){"trackers.rh",                  FT_DOUBLE, &trackers.rh,                 0};
  state->trackersPF[4] = (StateField){"trackers.rRoot",               FT_DOUBLE, &trackers.rRoot,              0};
  state->trackersPF[5] = (StateField){"trackers.rSoil",               FT_DOUBLE, &trackers.rSoil,              0};
  state->trackersPF[6] = (StateField){"trackers.rAboveground",        FT_DOUBLE, &trackers.rAboveground,       0};
  state->trackersPF[7] = (StateField){"trackers.npp",                 FT_DOUBLE, &trackers.npp,                0};
  state->trackersPF[8] = (StateField){"trackers.nee",                 FT_DOUBLE, &trackers.nee,                0};
  state->trackersPF[9] = (StateField){"trackers.woodCreation",        FT_DOUBLE, &trackers.woodCreation,       0};
  state->trackersPF[10] = (StateField){"trackers.gdd",                FT_DOUBLE, &trackers.gdd,                0};
  state->trackersPF[11] = (StateField){"trackers.evapotranspiration", FT_DOUBLE, &trackers.evapotranspiration, 0};
  state->trackersPF[12] = (StateField){"trackers.soilWetnessFrac",    FT_DOUBLE, &trackers.soilWetnessFrac,    0},
  state->trackersPF[13] = (StateField){"trackers.yearlyGpp",          FT_DOUBLE, &trackers.yearlyGpp,          0};
  state->trackersPF[14] = (StateField){"trackers.yearlyRtot",         FT_DOUBLE, &trackers.yearlyRtot,         0};
  state->trackersPF[15] = (StateField){"trackers.yearlyRa",           FT_DOUBLE, &trackers.yearlyRa,           0};
  state->trackersPF[16] = (StateField){"trackers.yearlyRh",           FT_DOUBLE, &trackers.yearlyRh,           0};
  state->trackersPF[17] = (StateField){"trackers.yearlyNpp",          FT_DOUBLE, &trackers.yearlyNpp,          0};
  state->trackersPF[18] = (StateField){"trackers.yearlyNee",          FT_DOUBLE, &trackers.yearlyNee,          0};
  state->trackersPF[19] = (StateField){"trackers.yearlyLitter",       FT_DOUBLE, &trackers.yearlyLitter,       0};
  state->trackersPF[20] = (StateField){"trackers.totGpp",             FT_DOUBLE, &trackers.totGpp,             0};
  state->trackersPF[21] = (StateField){"trackers.totRtot",            FT_DOUBLE, &trackers.totRtot,            0};
  state->trackersPF[22] = (StateField){"trackers.totRa",              FT_DOUBLE, &trackers.totRa,              0};
  state->trackersPF[23] = (StateField){"trackers.totRh",              FT_DOUBLE, &trackers.totRh,              0};
  state->trackersPF[24] = (StateField){"trackers.totNpp",             FT_DOUBLE, &trackers.totNpp,             0};
  state->trackersPF[25] = (StateField){"trackers.totNee",             FT_DOUBLE, &trackers.totNee,             0};
  state->trackersPF[26] = (StateField){"trackers.lastYear",           FT_INT,    &trackers.lastYear,           0};
  state->trackersPF[27] = (StateField){"trackers.methane",            FT_DOUBLE, &trackers.methane,            0};
  state->trackersPF[28] = (StateField){"trackers.n2o",                FT_DOUBLE, &trackers.n2o,                0};
  state->trackersPF[29] = (StateField){"trackers.nLeaching",          FT_DOUBLE, &trackers.nLeaching,          0};
  state->trackersPF[30] = (StateField){"trackers.nFixation",          FT_DOUBLE, &trackers.nFixation,          0};
  state->trackersPF[31] = (StateField){"trackers.nUptake",            FT_DOUBLE, &trackers.nUptake,            0};
  state->trackersPF[32] = (StateField){"trackers.invalid",            FT_INVALID, NULL, FIELD_INVALID};

  state->phenologyPF[0] = (StateField){"phenology.didLeafGrowth", FT_INT,     &phenologyTrackers.didLeafGrowth, 0};
  state->phenologyPF[1] = (StateField){"phenology.didLeafFall",   FT_INT,     &phenologyTrackers.didLeafFall,   0};
  state->phenologyPF[2] = (StateField){"phenology.lastYear",      FT_INT,     &phenologyTrackers.lastYear,      0};
  state->phenologyPF[3] = (StateField){"phenology.invalid",       FT_INVALID, NULL, FIELD_INVALID};

  state->eventPF[0] = (StateField){"event_trackers.d_till_mod", FT_DOUBLE,  &eventTrackers.d_till_mod, 0};
  state->eventPF[1] = (StateField){"event_trackers.invalid",    FT_INVALID, NULL, FIELD_INVALID};

  // meanNPP array handlers

  state->endPF[0] = (StateField){"end_restart", FT_INT, &endRestart, 0};
  // NOLINTEND
  // clang-format on
}

static void copyClimateSignature(RestartClimateSignature *dest,
                                 const ClimateNode *src) {
  dest->year = src->year;
  dest->day = src->day;
  dest->time = src->time;
  dest->length = src->length;
}

static int
climateTimestampIsAfterBoundary(const ClimateNode *actual,
                                const RestartClimateSignature *boundary) {
  if (actual->year != boundary->year) {
    return actual->year > boundary->year;
  }
  if (actual->day != boundary->day) {
    return actual->day > boundary->day;
  }
  return actual->time > (boundary->time + RESTART_FLOAT_EPSILON);
}

static int isLeapYear(int year) {
  return ((year % 4 == 0) && (year % 100 != 0)) || (year % 400 == 0);
}

static int daysInYear(int year) { return isLeapYear(year) ? 366 : 365; }

static void advanceOneDay(int *year, int *day) {
  ++(*day);
  if (*day > daysInYear(*year)) {
    *day = 1;
    ++(*year);
  }
}

static int dateIsBefore(int leftYear, int leftDay, int rightYear,
                        int rightDay) {
  if (leftYear != rightYear) {
    return leftYear < rightYear;
  }
  return leftDay < rightDay;
}

static void
validateCheckpointBoundaryForWrite(const char *restartOut,
                                   const RestartClimateSignature *boundary) {
  double stepHours = boundary->length * 24.0;
  if (stepHours <= RESTART_FLOAT_EPSILON) {
    logError("Cannot write restart checkpoint %s: non-positive timestep length "
             "at boundary (year=%d day=%d time=%.8f length=%.8f)\n",
             restartOut, boundary->year, boundary->day, boundary->time,
             boundary->length);
    exit(EXIT_CODE_BAD_RESTART_PARAMETER);
  }

  double hoursUntilMidnight = 24.0 - boundary->time;
  if (hoursUntilMidnight > (stepHours + RESTART_FLOAT_EPSILON)) {
    logWarning("Writing restart checkpoint %s even though the last timestep "
               "ends more than one timestep before midnight; this checkpoint "
               "should not be used for resume\n",
               restartOut);
    logWarning("Boundary timestep: year=%d day=%d time=%.8f length=%.8f\n",
               boundary->year, boundary->day, boundary->time, boundary->length);
  }
}

static void
validateCheckpointBoundaryForLoad(const char *restartIn,
                                  const RestartClimateSignature *boundary) {
  double stepHours = boundary->length * 24.0;
  if (stepHours <= RESTART_FLOAT_EPSILON) {
    logError("Restart boundary mismatch in %s: checkpoint boundary has "
             "non-positive timestep length (year=%d day=%d time=%.8f "
             "length=%.8f)\n",
             restartIn, boundary->year, boundary->day, boundary->time,
             boundary->length);
    exit(EXIT_CODE_BAD_RESTART_PARAMETER);
  }

  double hoursUntilMidnight = 24.0 - boundary->time;
  if (hoursUntilMidnight > (stepHours + RESTART_FLOAT_EPSILON)) {
    logError(
        "Restart boundary mismatch in %s: checkpoint boundary is more than "
        "one timestep before midnight\n",
        restartIn);
    logError("Checkpoint boundary: year=%d day=%d time=%.8f length=%.8f\n",
             boundary->year, boundary->day, boundary->time, boundary->length);
    exit(EXIT_CODE_BAD_RESTART_PARAMETER);
  }
}

// Assumes input char *dest is BUILD_INFO_BUFFER_SIZE in length
static void sanitizeBuildInfo(char *dest, const char *src) {
  size_t ind = 0;
  while (src[ind] != '\0' && ind < (BUILD_INFO_BUFFER_SIZE - 1)) {
    char c = src[ind];
    if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
      dest[ind] = '_';
    } else {
      dest[ind] = c;
    }
    ++ind;
  }
  dest[ind] = '\0';
}

static void parseError(const char *restartIn, const char *msg,
                       const char *key) {
  if (key != NULL) {
    logError("Restart parse error in %s: %s (%s)\n", restartIn, msg, key);
  } else {
    logError("Restart parse error in %s: %s\n", restartIn, msg);
  }
  exit(EXIT_CODE_BAD_RESTART_PARAMETER);
}

static void parseValueError(const char *restartIn, const char *key,
                            const char *value) {
  logError("Restart parse error in %s: invalid value '%s' for key '%s'\n",
           restartIn, value, key);
  exit(EXIT_CODE_BAD_RESTART_PARAMETER);
}

static long long parseLongLongStrict(const char *restartIn, const char *key,
                                     const char *value) {
  char *end = NULL;
  errno = 0;
  long long parsed = strtoll(value, &end, 10);
  if (end == value || *end != '\0' || errno == ERANGE) {
    parseValueError(restartIn, key, value);
  }
  return parsed;
}

static int parseIntStrict(const char *restartIn, const char *key,
                          const char *value) {
  long long parsed = parseLongLongStrict(restartIn, key, value);
  if (parsed < INT_MIN || parsed > INT_MAX) {
    parseValueError(restartIn, key, value);
  }
  return (int)parsed;
}

static double parseDoubleStrict(const char *restartIn, const char *key,
                                const char *value) {
  char *end = NULL;
  double parsed = strtod(value, &end);
  if (end == value || *end != '\0' || !isfinite(parsed)) {
    parseValueError(restartIn, key, value);
  }
  return parsed;
}

static void validateSchemaLayoutValue(const char *restartIn, const char *key,
                                      const char *value, long long expected) {
  long long parsed = parseIntStrict(restartIn, key, value);
  if (parsed != expected) {
    logError(
        "Restart schema layout mismatch in %s: key=%s found=%d expected=%d "
        "(schema %s)\n",
        restartIn, key, parsed, expected, RESTART_SCHEMA_VERSION);
    exit(EXIT_CODE_BAD_RESTART_PARAMETER);
  }
}

static void checkLineLength(const char *line, size_t lineLen,
                            const char *restartIn, FILE *in) {
  if (lineLen > 0 && line[lineLen - 1] != '\n' && !feof(in)) {
    parseError(restartIn, "line too long or truncated", NULL);
  }
}

void setFieldValue(StateField *sf, const void *valPtr) {
  switch (sf->type) {
    case FT_LONGLONG:
      *(long long *)sf->value = *(long long *)valPtr;
      break;
    case FT_DOUBLE:
      *(double *)sf->value = *(double *)valPtr;
      break;
    case FT_INT:
      *(int *)sf->value = *(int *)valPtr;
      break;
    case FT_CHAR: {
      // A little more care with this one - but we know this field has not been
      // "seen" yet
      size_t len = (size_t)sf->seen;
      // memset is needed here because the static char mem will be used more
      // than once when we have both load and write
      memset(sf->value, 0, sizeof(char) * len);
      strncpy((char *)sf->value, (char *)valPtr, len);
    } break;
    case FT_SPECIAL:
    case FT_INVALID:
      // This shouldn't happen, even with missing restart updates
      logInternalError("Restart parse error, found unexpected field type %d "
                       "(set)\n",
                       sf->type);
      exit(EXIT_CODE_INTERNAL_ERROR);
  }
}

void checkSeen(StateField *sf, const char *restartIn, const char *key) {
  if (sf->seen == FIELD_SEEN) {
    logError("Restart parse error in %s: duplicate key '%s'\n", restartIn, key);
    exit(EXIT_CODE_BAD_RESTART_PARAMETER);
  }
}

void setSeen(StateField *sf) { sf->seen = FIELD_SEEN; }

int checkSeenAndSet(StateField *sf, const char *restartIn, const char *key,
                    const char *value) {
  int hit = 0;
  if (strcmp(sf->key, key) == 0) {
    checkSeen(sf, restartIn, key);
    hit = 1;
    switch (sf->type) {
      case FT_LONGLONG: {
        long long val = parseLongLongStrict(restartIn, key, value);
        setFieldValue(sf, &val);
      } break;
      case FT_INT: {
        int val = parseIntStrict(restartIn, key, value);
        setFieldValue(sf, &val);
      } break;
      case FT_DOUBLE: {
        double val = parseDoubleStrict(restartIn, key, value);
        setFieldValue(sf, &val);
      } break;
      case FT_CHAR: {
        setFieldValue(sf, value);
      } break;
      case FT_SPECIAL:
      case FT_INVALID:
        // This shouldn't happen, even with missing restart updates
        logInternalError("Restart parse error, found unexpected field type %d "
                         "(check)\n",
                         sf->type);
        exit(EXIT_CODE_INTERNAL_ERROR);
    }
    setSeen(sf);
  }

  return hit;
}

int checkAndSetBatch(StateField *sf, const char *prefix, int numFields,
                     const char *restartIn, const char *key,
                     const char *value) {
  int hit = 0;
  int keyInd;
  if (strncmp(key, prefix, strlen(prefix)) == 0) {
    for (keyInd = 0; keyInd < numFields; ++keyInd) {
      if (checkSeenAndSet(&sf[keyInd], restartIn, key, value)) {
        hit = 1;
        continue;
      }
    }
  }

  return hit;
}

int checkAndValidateSchema(StateField *sf, const char *restartIn,
                           const char *key, const char *value) {
  int hit = 0;
  int keyInd;
  const char *prefix = "schema_layout.";
  if (strncmp(key, prefix, strlen(prefix)) == 0) {
    for (keyInd = 0; keyInd < NUM_SCHEMA_FIELDS; ++keyInd) {
      if (strcmp(sf[keyInd].key, key) == 0) {
        checkSeen(&sf[keyInd], restartIn, key);
        int exp = sf[keyInd].seen;
        validateSchemaLayoutValue(restartIn, key, value, exp);
        setSeen(&sf[keyInd]);
        hit = 1;
        continue;
      }
    }
  }

  return hit;
}

void verifySeenBatch(StateField *sf, int numFields, const char *restartIn) {
  for (int ind = 0; ind < numFields; ++ind) {
    if (sf[ind].seen != FIELD_SEEN) {
      parseError(restartIn, "missing required key", sf[ind].key);
    }
  }
}

// Still needed for meanNPP array handling
void markSeen(int *seen, const char *restartIn, const char *key) {
  if (*seen == FIELD_SEEN) {
    logError("Restart parse error in %s: duplicate key '%s'\n", restartIn, key);
    exit(EXIT_CODE_BAD_RESTART_PARAMETER);
  }
  *seen = FIELD_SEEN;
}

static void readRestartState(const char *restartIn, RestartState *state,
                             MeanTracker *meanNPP) {
  FILE *in = openFile(restartIn, "r");

  char firstLine[256];
  if (fgets(firstLine, sizeof(firstLine), in) == NULL) {
    parseError(restartIn, "missing header line", NULL);
  }
  checkLineLength(firstLine, strlen(firstLine), restartIn, in);

  char magic[64];
  char schemaVersion[16];
  if (sscanf(firstLine, "%63s %15s", magic, schemaVersion) != 2) {
    parseError(restartIn, "invalid header line", NULL);
  }

  if (strcmp(magic, RESTART_MAGIC) != 0) {
    logError("Restart file %s has invalid magic header\n", restartIn);
    exit(EXIT_CODE_BAD_RESTART_PARAMETER);
  }
  if (strcmp(schemaVersion, RESTART_SCHEMA_VERSION) != 0) {
    logError("Restart schema mismatch in %s: found %s expected %s\n", restartIn,
             schemaVersion, RESTART_SCHEMA_VERSION);
    exit(EXIT_CODE_BAD_RESTART_PARAMETER);
  }

  int meanLength = meanNPP->length;
  int *seenMeanValues = (int *)calloc((size_t)meanLength, sizeof(int));
  int *seenMeanWeights = (int *)calloc((size_t)meanLength, sizeof(int));
  if (seenMeanValues == NULL || seenMeanWeights == NULL) {
    parseError(restartIn, "unable to allocate mean tracker memory", NULL);
  }

  char line[4096];
  char key[128];
  char value[2048];
  char extra[32];
  while (fgets(line, sizeof(line), in) != NULL) {
    size_t lineLen = strlen(line);
    checkLineLength(line, lineLen, restartIn, in);
    int seenEndRestart = (state->endPF[0].seen == FIELD_SEEN);
    if (seenEndRestart) {
      logInfo("Ignoring extra lines after end_restart in %s\n", restartIn);
      break;
    }

    int n = sscanf(line, " %127s %2047s %31s", key, value, extra);
    if (n <= 0) {
      continue;
    }
    if (n != 2) {
      parseError(restartIn, "line must contain exactly '<key> <value>'", NULL);
    }

    // Search for the key in our StateField structs
    if (checkAndSetBatch(state->metaPF, "meta_info.", NUM_META_FIELDS,
                         restartIn, key, value)) {
      continue;
    }
    if (checkAndValidateSchema(state->schemaPF, restartIn, key, value)) {
      continue;
    }
    if (checkAndSetBatch(state->flagsPF, "flags.", NUM_CONTEXT_MODEL_FLAGS,
                         restartIn, key, value)) {
      continue;
    }
    if (checkAndSetBatch(state->boundaryPF, "boundary.",
                         NUM_CLIMATE_SIGNATURE_FIELDS, restartIn, key, value)) {
      continue;
    }
    if (checkAndSetBatch(state->nppPF, "mean.npp.", NUM_MEAN_META_FIELDS,
                         restartIn, key, value)) {
      continue;
    }
    if (checkAndSetBatch(state->enviPF, "envi.", NUM_ENVI_FIELDS, restartIn,
                         key, value)) {
      continue;
    }
    if (checkAndSetBatch(state->trackersPF, "trackers.", NUM_TRACKER_FIELDS,
                         restartIn, key, value)) {
      continue;
    }
    if (checkAndSetBatch(state->phenologyPF, "phenology.",
                         NUM_PHENOLOGY_TRACKERS_FIELDS, restartIn, key,
                         value)) {
      continue;
    }
    if (checkAndSetBatch(state->eventPF, "event_trackers.",
                         NUM_EVENT_TRACKERS_FIELDS, restartIn, key, value)) {
      continue;
    }
    if (checkAndSetBatch(state->endPF, "end_restart", NUM_END_FIELDS, restartIn,
                         key, value)) {
      continue;
    }

    // Special handling for meanNPP values
    if (strncmp(key, "mean.npp.values.", strlen("mean.npp.values.")) == 0) {
      int idx =
          parseIntStrict(restartIn, key, key + strlen("mean.npp.values."));
      if (idx < 0 || idx >= meanLength) {
        parseError(restartIn, "mean.npp.values index out of range", key);
      }
      markSeen(&(seenMeanValues[idx]), restartIn, key);
      meanNPP->values[idx] = parseDoubleStrict(restartIn, key, value);
      continue;
    }

    // Special handling for meanNPP weights
    if (strncmp(key, "mean.npp.weights.", strlen("mean.npp.weights.")) == 0) {
      int idx =
          parseIntStrict(restartIn, key, key + strlen("mean.npp.weights."));
      if (idx < 0 || idx >= meanLength) {
        parseError(restartIn, "mean.npp.weights index out of range", key);
      }
      markSeen(&(seenMeanWeights[idx]), restartIn, key);
      meanNPP->weights[idx] = parseDoubleStrict(restartIn, key, value);
      continue;
    }

    logError("Restart parse error in %s: unknown key '%s'\n", restartIn, key);
    exit(EXIT_CODE_BAD_RESTART_PARAMETER);
  }

  fclose(in);

  verifySeenBatch(state->metaPF, NUM_META_FIELDS, restartIn);
  verifySeenBatch(state->schemaPF, NUM_SCHEMA_FIELDS, restartIn);
  verifySeenBatch(state->flagsPF, NUM_CONTEXT_MODEL_FLAGS, restartIn);
  verifySeenBatch(state->boundaryPF, NUM_CLIMATE_SIGNATURE_FIELDS, restartIn);
  verifySeenBatch(state->nppPF, NUM_MEAN_META_FIELDS, restartIn);
  verifySeenBatch(state->enviPF, NUM_ENVI_FIELDS, restartIn);
  verifySeenBatch(state->trackersPF, NUM_TRACKER_FIELDS, restartIn);
  verifySeenBatch(state->phenologyPF, NUM_PHENOLOGY_TRACKERS_FIELDS, restartIn);
  verifySeenBatch(state->eventPF, NUM_EVENT_TRACKERS_FIELDS, restartIn);
  verifySeenBatch(state->endPF, NUM_END_FIELDS, restartIn);

  // meanNPP checks
  for (int i = 0; i < meanLength; ++i) {
    if (!seenMeanValues[i]) {
      parseError(restartIn, "mean.npp.values array is incomplete", NULL);
    }
  }
  for (int i = 0; i < meanLength; ++i) {
    if (!seenMeanWeights[i]) {
      parseError(restartIn, "mean.npp.weights array is incomplete", NULL);
    }
  }

  free(seenMeanValues);
  free(seenMeanWeights);
}

static void writeKeysBatch(FILE *out, const StateField *sf, int numFields) {
  for (int ind = 0; ind < numFields; ++ind) {
    StateField curSF = sf[ind];
    switch (curSF.type) {
      case FT_DOUBLE:
        fprintf(out, "%s %.17g\n", curSF.key, *(double *)curSF.value);
        break;
      case FT_INT:
        fprintf(out, "%s %d\n", curSF.key, *(int *)curSF.value);
        break;
      case FT_CHAR:
        fprintf(out, "%s %s\n", curSF.key, (char *)curSF.value);
        break;
      case FT_LONGLONG:
        fprintf(out, "%s %lld\n", curSF.key, *(long long *)curSF.value);
        break;
      case FT_SPECIAL:  // FT_SPECIAL writes its 'seen' value
        fprintf(out, "%s %d\n", curSF.key, curSF.seen);
        break;
      case FT_INVALID:
        logInternalError("Attempted to write invalid key %s, restart.c likely "
                         "needs an update\n",
                         curSF.key);
        exit(EXIT_CODE_INTERNAL_ERROR);
        break;
    }
  }
}

static void writeRestartState(const char *restartOut, const RestartState *state,
                              const MeanTracker *meanNPP) {
  FILE *out = openFile(restartOut, "w");

  // Magic header
  fprintf(out, "%s %s\n", RESTART_MAGIC, RESTART_SCHEMA_VERSION);

  // Schema batches
  writeKeysBatch(out, state->metaPF, NUM_META_FIELDS);
  writeKeysBatch(out, state->schemaPF, NUM_SCHEMA_FIELDS);
  fprintf(out, "\n");
  writeKeysBatch(out, state->flagsPF, NUM_CONTEXT_MODEL_FLAGS);
  fprintf(out, "\n");
  writeKeysBatch(out, state->boundaryPF, NUM_CLIMATE_SIGNATURE_FIELDS);
  fprintf(out, "\n");
  writeKeysBatch(out, state->enviPF, NUM_ENVI_FIELDS);
  fprintf(out, "\n");
  writeKeysBatch(out, state->trackersPF, NUM_TRACKER_FIELDS);
  fprintf(out, "\n");
  writeKeysBatch(out, state->phenologyPF, NUM_PHENOLOGY_TRACKERS_FIELDS);
  fprintf(out, "\n");
  writeKeysBatch(out, state->eventPF, NUM_EVENT_TRACKERS_FIELDS);
  fprintf(out, "\n");
  writeKeysBatch(out, state->nppPF, NUM_MEAN_META_FIELDS);
  fprintf(out, "\n");

  for (int i = 0; i < meanNPP->length; ++i) {
    fprintf(out, "mean.npp.values.%d %.17g\n", i, meanNPP->values[i]);
  }
  fprintf(out, "\n");

  for (int i = 0; i < meanNPP->length; ++i) {
    fprintf(out, "mean.npp.weights.%d %.17g\n", i, meanNPP->weights[i]);
  }
  fprintf(out, "\n");

  fprintf(out, "end_restart 1\n");

  fclose(out);
}

static void checkRestartContextCompatibility(void) {
  int mismatch = 0;

  mismatch |= (ctx.events != modelFlags.events);
  mismatch |= (ctx.gdd != modelFlags.gdd);
  mismatch |= (ctx.growthResp != modelFlags.growthResp);
  mismatch |= (ctx.leafWater != modelFlags.leafWater);
  mismatch |= (ctx.litterPool != modelFlags.litterPool);
  mismatch |= (ctx.snow != modelFlags.snow);
  mismatch |= (ctx.soilPhenol != modelFlags.soilPhenol);
  mismatch |= (ctx.waterHResp != modelFlags.waterHResp);
  mismatch |= (ctx.nitrogenCycle != modelFlags.nitrogenCycle);
  mismatch |= (ctx.anaerobic != modelFlags.anaerobic);

  if (mismatch) {
    logError("Restart context mismatch: model flags must match checkpoint "
             "exactly\n");
    exit(EXIT_CODE_BAD_RESTART_PARAMETER);
  }
}

static void validateRestartModelBuild(void) {
  char currentBuildInfo[BUILD_INFO_BUFFER_SIZE];
  sanitizeBuildInfo(currentBuildInfo, VERSION_STRING);

  if (strcmp(modelVersion, NUMERIC_VERSION) != 0) {
    logError("Restart model version mismatch: checkpoint=%s current=%s\n",
             modelVersion, NUMERIC_VERSION);
    exit(EXIT_CODE_BAD_RESTART_PARAMETER);
  }

  if (strcmp(buildInfo, currentBuildInfo) != 0) {
    logWarning("Restart build info mismatch: checkpoint=%s current=%s\n",
               buildInfo, currentBuildInfo);
  }
}

static void validateRestartBoundary(void) {
  if (climate == NULL) {
    logError("Cannot restart: climate forcing has no records\n");
    exit(EXIT_CODE_INPUT_FILE_ERROR);
  }

  if (!climateTimestampIsAfterBoundary(climate, &(boundaryClimate))) {
    logError("Restart boundary mismatch: first climate timestamp does not "
             "follow checkpoint boundary timestamp\n");
    logError("Checkpoint boundary: year=%d day=%d time=%.8f\n",
             boundaryClimate.year, boundaryClimate.day, boundaryClimate.time);
    logError("Found:    year=%d day=%d time=%.8f\n", climate->year,
             climate->day, climate->time);
    exit(EXIT_CODE_BAD_RESTART_PARAMETER);
  }

  double firstStepHours = climate->length * 24.0;
  if (firstStepHours <= RESTART_FLOAT_EPSILON) {
    logError("Cannot restart: first climate timestep length is non-positive "
             "(year=%d day=%d time=%.8f length=%.8f)\n",
             climate->year, climate->day, climate->time, climate->length);
    exit(EXIT_CODE_BAD_RESTART_PARAMETER);
  }

  int expectedYear = boundaryClimate.year;
  int expectedDay = boundaryClimate.day;
  advanceOneDay(&expectedYear, &expectedDay);

  if (climate->year != expectedYear || climate->day != expectedDay ||
      climate->time > (firstStepHours + RESTART_FLOAT_EPSILON)) {
    logError("Restart boundary mismatch: resumed segment must start within one "
             "timestep after midnight checkpoint boundary\n");
    logError("Expected start on year=%d day=%d with time<=%.8f; found "
             "year=%d day=%d time=%.8f length=%.8f\n",
             expectedYear, expectedDay, firstStepHours, climate->year,
             climate->day, climate->time, climate->length);
    exit(EXIT_CODE_BAD_RESTART_PARAMETER);
  }
}

static void validateRestartEventBoundary(void) {
  if (!ctx.events || gEvent == NULL || climate == NULL) {
    return;
  }

  if (dateIsBefore(gEvent->year, gEvent->day, climate->year, climate->day)) {
    logError("Restart event boundary mismatch: first event (year=%d day=%d) is "
             "before resumed climate start (year=%d day=%d)\n",
             gEvent->year, gEvent->day, climate->year, climate->day);
    logError("Checkpoint boundary was year=%d day=%d; event files must be "
             "segmented to the same boundaries as climate segments\n",
             boundaryClimate.year, boundaryClimate.day);
    exit(EXIT_CODE_BAD_RESTART_PARAMETER);
  }
}

void restartResetRunState(void) {
  processedStepCount = 0;
  lastProcessedClimateStep = NULL;
}

void restartNoteProcessedClimateStep(const ClimateNode *climateStep) {
  lastProcessedClimateStep = climateStep;
  ++processedStepCount;
}

void restartWriteCheckpoint(const char *restartOut, MeanTracker *meanNPP) {
  if (lastProcessedClimateStep == NULL) {
    logError("Cannot write restart checkpoint %s: no timestep processed\n",
             restartOut);
    exit(EXIT_CODE_BAD_RESTART_PARAMETER);
  }
  // Need to copy to restart versions of some state:
  // 1. climate
  // 2. model version
  // 3. build info
  // 4. UTC epoch
  // 5. model flags

  // 1. climate
  copyClimateSignature(&boundaryClimate, lastProcessedClimateStep);
  validateCheckpointBoundaryForWrite(restartOut, &boundaryClimate);

  RestartState state;
  initResetState(&state, meanNPP);

  // 2, 3, 4: model version, build info, UTC epoch
  strncpy(modelVersion, NUMERIC_VERSION, MODEL_VERSION_BUFFER_SIZE - 1);
  sanitizeBuildInfo(buildInfo, VERSION_STRING);
  checkpointUTCEpoch = (long long)time(NULL);

  // 5. model flags
  int numFlagsSet = 0;
  modelFlags.events = ctx.events;
  ++numFlagsSet;
  modelFlags.gdd = ctx.gdd;
  ++numFlagsSet;
  modelFlags.growthResp = ctx.growthResp;
  ++numFlagsSet;
  modelFlags.leafWater = ctx.leafWater;
  ++numFlagsSet;
  modelFlags.litterPool = ctx.litterPool;
  ++numFlagsSet;
  modelFlags.snow = ctx.snow;
  ++numFlagsSet;
  modelFlags.soilPhenol = ctx.soilPhenol;
  ++numFlagsSet;
  modelFlags.waterHResp = ctx.waterHResp;
  ++numFlagsSet;
  modelFlags.nitrogenCycle = ctx.nitrogenCycle;
  ++numFlagsSet;
  modelFlags.anaerobic = ctx.anaerobic;
  ++numFlagsSet;
  if (numFlagsSet != NUM_CONTEXT_MODEL_FLAGS) {
    logInternalError("Not all model flags set while writing checkpoint\n");
    exit(EXIT_CODE_INTERNAL_ERROR);
  }

  writeRestartState(restartOut, &state, meanNPP);
}

void restartLoadCheckpoint(const char *restartIn, MeanTracker *meanNPP) {
  RestartState state;
  initResetState(&state, meanNPP);

  readRestartState(restartIn, &state, meanNPP);

  validateCheckpointBoundaryForLoad(restartIn, &boundaryClimate);
  checkRestartContextCompatibility();
  validateRestartModelBuild();
  validateRestartBoundary();
  validateRestartEventBoundary();

  if (meanNPP->start < 0 || meanNPP->start >= meanNPP->length ||
      meanNPP->last < 0 || meanNPP->last >= meanNPP->length) {
    logError("Restart mean-tracker cursor out of range in %s\n", restartIn);
    exit(EXIT_CODE_BAD_RESTART_PARAMETER);
  }

  lastProcessedClimateStep = NULL;
}

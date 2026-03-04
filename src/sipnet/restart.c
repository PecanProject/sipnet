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

#define RESTART_SCHEMA_LAYOUT_ENVI_SIZE 96
#define RESTART_SCHEMA_LAYOUT_TRACKERS_SIZE 224
#define RESTART_SCHEMA_LAYOUT_PHENOLOGY_TRACKERS_SIZE 12
#define RESTART_SCHEMA_LAYOUT_EVENT_TRACKERS_SIZE 8

typedef struct RestartSerializedTrackersV1 {
  double gpp;
  double rtot;
  double ra;
  double rh;
  double npp;
  double nee;
  double yearlyGpp;
  double yearlyRtot;
  double yearlyRa;
  double yearlyRh;
  double yearlyNpp;
  double yearlyNee;
  double totGpp;
  double totRtot;
  double totRa;
  double totRh;
  double totNpp;
  double totNee;
  double evapotranspiration;
  double soilWetnessFrac;
  double rRoot;
  double rSoil;
  double rAboveground;
  double yearlyLitter;
  double woodCreation;
  double n2o;
  double gdd;
  int lastYear;
} RestartSerializedTrackersV1;

_Static_assert(sizeof(Envi) == RESTART_SCHEMA_LAYOUT_ENVI_SIZE,
               "Restart schema 1.0 drift: Envi changed; bump restart schema "
               "version and update schema_layout.* checks");
_Static_assert(sizeof(RestartSerializedTrackersV1) ==
                   RESTART_SCHEMA_LAYOUT_TRACKERS_SIZE,
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

typedef struct RestartClimateSignature {
  int year;
  int day;
  double time;
  double length;
} RestartClimateSignature;

typedef struct RestartStateV1 {
  char modelVersion[32];
  char buildInfo[96];
  long long checkpointUtcEpoch;
  long long processedSteps;

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

  RestartClimateSignature boundaryClimate;

  // Mean-tracker metadata
  int meanLength;
  double meanTotWeight;
  int meanStart;
  int meanLast;
  double meanSum;

  Envi envi;
  Trackers trackers;
  PhenologyTrackers phenologyTrackers;
  EventTrackers eventTrackers;
} RestartStateV1;

static long long processedStepCount = 0;
static RestartClimateSignature lastProcessedClimate;
static int hasLastProcessedClimate = 0;

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
    exit(EXIT_CODE_BAD_PARAMETER_VALUE);
  }

  double hoursUntilMidnight = 24.0 - boundary->time;
  if (hoursUntilMidnight > (stepHours + RESTART_FLOAT_EPSILON)) {
    logError("Cannot write restart checkpoint %s: last timestep ends more than "
             "one timestep before midnight\n",
             restartOut);
    logError("Boundary timestep: year=%d day=%d time=%.8f length=%.8f\n",
             boundary->year, boundary->day, boundary->time, boundary->length);
    exit(EXIT_CODE_BAD_PARAMETER_VALUE);
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
    exit(EXIT_CODE_BAD_PARAMETER_VALUE);
  }

  double hoursUntilMidnight = 24.0 - boundary->time;
  if (hoursUntilMidnight > (stepHours + RESTART_FLOAT_EPSILON)) {
    logError(
        "Restart boundary mismatch in %s: checkpoint boundary is more than "
        "one timestep before midnight\n",
        restartIn);
    logError("Checkpoint boundary: year=%d day=%d time=%.8f length=%.8f\n",
             boundary->year, boundary->day, boundary->time, boundary->length);
    exit(EXIT_CODE_BAD_PARAMETER_VALUE);
  }
}

static void sanitizeBuildInfo(char *dest, size_t destLen, const char *src) {
  if (destLen == 0) {
    return;
  }

  size_t ind = 0;
  while (src[ind] != '\0' && ind < (destLen - 1)) {
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
  exit(EXIT_CODE_BAD_PARAMETER_VALUE);
}

static void parseValueError(const char *restartIn, const char *key,
                            const char *value) {
  logError("Restart parse error in %s: invalid value '%s' for key '%s'\n",
           restartIn, value, key);
  exit(EXIT_CODE_BAD_PARAMETER_VALUE);
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
  long long parsed = parseLongLongStrict(restartIn, key, value);
  if (parsed != expected) {
    logError(
        "Restart schema layout mismatch in %s: key=%s found=%lld expected=%lld "
        "(schema %s)\n",
        restartIn, key, parsed, expected, RESTART_SCHEMA_VERSION);
    exit(EXIT_CODE_BAD_PARAMETER_VALUE);
  }
}

static void checkLineLength(const char *line, size_t lineLen,
                            const char *restartIn, FILE *in) {
  if (lineLen > 0 && line[lineLen - 1] != '\n' && !feof(in)) {
    parseError(restartIn, "line too long or truncated", NULL);
  }
}

static void markSeen(int *seen, const char *restartIn, const char *key) {
  if (*seen) {
    logError("Restart parse error in %s: duplicate key '%s'\n", restartIn, key);
    exit(EXIT_CODE_BAD_PARAMETER_VALUE);
  }
  *seen = 1;
}

static void readRestartState(const char *restartIn, RestartStateV1 *state,
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
    exit(EXIT_CODE_BAD_PARAMETER_VALUE);
  }
  if (strcmp(schemaVersion, RESTART_SCHEMA_VERSION) != 0) {
    logError("Restart schema mismatch in %s: found %s expected %s\n", restartIn,
             schemaVersion, RESTART_SCHEMA_VERSION);
    exit(EXIT_CODE_BAD_PARAMETER_VALUE);
  }

  int seenModelVersion = 0;
  int seenBuildInfo = 0;
  int seenCheckpointUtcEpoch = 0;
  int seenProcessedSteps = 0;
  int seenSchemaLayout[4] = {0};

  int seenFlags[10] = {0};
  int seenBoundary[4] = {0};
  int seenMeanMeta[5] = {0};
  int seenEnvi[12] = {0};
  int seenTrackers[28] = {0};
  int seenPhenology[3] = {0};
  int seenEventTrackers = 0;

  int seenMeanValuesLength = 0;
  int seenMeanWeightsLength = 0;
  int meanValuesLength = -1;
  int meanWeightsLength = -1;
  int *seenMeanValues = NULL;
  int *seenMeanWeights = NULL;
  int seenEndRestart = 0;

  char line[4096];
  while (fgets(line, sizeof(line), in) != NULL) {
    size_t lineLen = strlen(line);
    checkLineLength(line, lineLen, restartIn, in);

    char key[128];
    char value[2048];
    char extra[32];
    int n = sscanf(line, " %127s %2047s %31s", key, value, extra);
    if (n <= 0) {
      continue;
    }
    if (n != 2) {
      parseError(restartIn, "line must contain exactly '<key> <value>'", NULL);
    }
    if (seenEndRestart) {
      parseError(restartIn, "unexpected content after end_restart", key);
    }

    if (strcmp(key, "model_version") == 0) {
      markSeen(&seenModelVersion, restartIn, key);
      strncpy(state->modelVersion, value, sizeof(state->modelVersion) - 1);
      continue;
    }
    if (strcmp(key, "build_info") == 0) {
      markSeen(&seenBuildInfo, restartIn, key);
      strncpy(state->buildInfo, value, sizeof(state->buildInfo) - 1);
      continue;
    }
    if (strcmp(key, "checkpoint_utc_epoch") == 0) {
      markSeen(&seenCheckpointUtcEpoch, restartIn, key);
      state->checkpointUtcEpoch = parseLongLongStrict(restartIn, key, value);
      continue;
    }
    if (strcmp(key, "processed_steps") == 0) {
      markSeen(&seenProcessedSteps, restartIn, key);
      state->processedSteps = parseLongLongStrict(restartIn, key, value);
      continue;
    }
    if (strcmp(key, "schema_layout.envi_size") == 0) {
      markSeen(&(seenSchemaLayout[0]), restartIn, key);
      validateSchemaLayoutValue(restartIn, key, value,
                                RESTART_SCHEMA_LAYOUT_ENVI_SIZE);
      continue;
    }
    if (strcmp(key, "schema_layout.trackers_size") == 0) {
      markSeen(&(seenSchemaLayout[1]), restartIn, key);
      validateSchemaLayoutValue(restartIn, key, value,
                                RESTART_SCHEMA_LAYOUT_TRACKERS_SIZE);
      continue;
    }
    if (strcmp(key, "schema_layout.phenology_trackers_size") == 0) {
      markSeen(&(seenSchemaLayout[2]), restartIn, key);
      validateSchemaLayoutValue(restartIn, key, value,
                                RESTART_SCHEMA_LAYOUT_PHENOLOGY_TRACKERS_SIZE);
      continue;
    }
    if (strcmp(key, "schema_layout.event_trackers_size") == 0) {
      markSeen(&(seenSchemaLayout[3]), restartIn, key);
      validateSchemaLayoutValue(restartIn, key, value,
                                RESTART_SCHEMA_LAYOUT_EVENT_TRACKERS_SIZE);
      continue;
    }

    if (strcmp(key, "flags.events") == 0) {
      markSeen(&(seenFlags[0]), restartIn, key);
      state->events = parseIntStrict(restartIn, key, value);
      continue;
    }
    if (strcmp(key, "flags.gdd") == 0) {
      markSeen(&(seenFlags[1]), restartIn, key);
      state->gdd = parseIntStrict(restartIn, key, value);
      continue;
    }
    if (strcmp(key, "flags.growthResp") == 0) {
      markSeen(&(seenFlags[2]), restartIn, key);
      state->growthResp = parseIntStrict(restartIn, key, value);
      continue;
    }
    if (strcmp(key, "flags.leafWater") == 0) {
      markSeen(&(seenFlags[3]), restartIn, key);
      state->leafWater = parseIntStrict(restartIn, key, value);
      continue;
    }
    if (strcmp(key, "flags.litterPool") == 0) {
      markSeen(&(seenFlags[4]), restartIn, key);
      state->litterPool = parseIntStrict(restartIn, key, value);
      continue;
    }
    if (strcmp(key, "flags.snow") == 0) {
      markSeen(&(seenFlags[5]), restartIn, key);
      state->snow = parseIntStrict(restartIn, key, value);
      continue;
    }
    if (strcmp(key, "flags.soilPhenol") == 0) {
      markSeen(&(seenFlags[6]), restartIn, key);
      state->soilPhenol = parseIntStrict(restartIn, key, value);
      continue;
    }
    if (strcmp(key, "flags.waterHResp") == 0) {
      markSeen(&(seenFlags[7]), restartIn, key);
      state->waterHResp = parseIntStrict(restartIn, key, value);
      continue;
    }
    if (strcmp(key, "flags.nitrogenCycle") == 0) {
      markSeen(&(seenFlags[8]), restartIn, key);
      state->nitrogenCycle = parseIntStrict(restartIn, key, value);
      continue;
    }
    if (strcmp(key, "flags.anaerobic") == 0) {
      markSeen(&(seenFlags[9]), restartIn, key);
      state->anaerobic = parseIntStrict(restartIn, key, value);
      continue;
    }

    if (strcmp(key, "boundary.year") == 0) {
      markSeen(&(seenBoundary[0]), restartIn, key);
      state->boundaryClimate.year = parseIntStrict(restartIn, key, value);
      continue;
    }
    if (strcmp(key, "boundary.day") == 0) {
      markSeen(&(seenBoundary[1]), restartIn, key);
      state->boundaryClimate.day = parseIntStrict(restartIn, key, value);
      continue;
    }
    if (strcmp(key, "boundary.time") == 0) {
      markSeen(&(seenBoundary[2]), restartIn, key);
      state->boundaryClimate.time = parseDoubleStrict(restartIn, key, value);
      continue;
    }
    if (strcmp(key, "boundary.length") == 0) {
      markSeen(&(seenBoundary[3]), restartIn, key);
      state->boundaryClimate.length = parseDoubleStrict(restartIn, key, value);
      continue;
    }
    if (strcmp(key, "mean.npp.length") == 0) {
      markSeen(&(seenMeanMeta[0]), restartIn, key);
      state->meanLength = parseIntStrict(restartIn, key, value);
      continue;
    }
    if (strcmp(key, "mean.npp.totWeight") == 0) {
      markSeen(&(seenMeanMeta[1]), restartIn, key);
      state->meanTotWeight = parseDoubleStrict(restartIn, key, value);
      continue;
    }
    if (strcmp(key, "mean.npp.start") == 0) {
      markSeen(&(seenMeanMeta[2]), restartIn, key);
      state->meanStart = parseIntStrict(restartIn, key, value);
      continue;
    }
    if (strcmp(key, "mean.npp.last") == 0) {
      markSeen(&(seenMeanMeta[3]), restartIn, key);
      state->meanLast = parseIntStrict(restartIn, key, value);
      continue;
    }
    if (strcmp(key, "mean.npp.sum") == 0) {
      markSeen(&(seenMeanMeta[4]), restartIn, key);
      state->meanSum = parseDoubleStrict(restartIn, key, value);
      continue;
    }

    if (strcmp(key, "envi.plantWoodC") == 0) {
      markSeen(&(seenEnvi[0]), restartIn, key);
      state->envi.plantWoodC = parseDoubleStrict(restartIn, key, value);
      continue;
    }
    if (strcmp(key, "envi.plantLeafC") == 0) {
      markSeen(&(seenEnvi[1]), restartIn, key);
      state->envi.plantLeafC = parseDoubleStrict(restartIn, key, value);
      continue;
    }
    if (strcmp(key, "envi.soilC") == 0) {
      markSeen(&(seenEnvi[2]), restartIn, key);
      state->envi.soilC = parseDoubleStrict(restartIn, key, value);
      continue;
    }
    if (strcmp(key, "envi.soilWater") == 0) {
      markSeen(&(seenEnvi[3]), restartIn, key);
      state->envi.soilWater = parseDoubleStrict(restartIn, key, value);
      continue;
    }
    if (strcmp(key, "envi.litterC") == 0) {
      markSeen(&(seenEnvi[4]), restartIn, key);
      state->envi.litterC = parseDoubleStrict(restartIn, key, value);
      continue;
    }
    if (strcmp(key, "envi.snow") == 0) {
      markSeen(&(seenEnvi[5]), restartIn, key);
      state->envi.snow = parseDoubleStrict(restartIn, key, value);
      continue;
    }
    if (strcmp(key, "envi.coarseRootC") == 0) {
      markSeen(&(seenEnvi[6]), restartIn, key);
      state->envi.coarseRootC = parseDoubleStrict(restartIn, key, value);
      continue;
    }
    if (strcmp(key, "envi.fineRootC") == 0) {
      markSeen(&(seenEnvi[7]), restartIn, key);
      state->envi.fineRootC = parseDoubleStrict(restartIn, key, value);
      continue;
    }
    if (strcmp(key, "envi.minN") == 0) {
      markSeen(&(seenEnvi[8]), restartIn, key);
      state->envi.minN = parseDoubleStrict(restartIn, key, value);
      continue;
    }
    if (strcmp(key, "envi.soilOrgN") == 0) {
      markSeen(&(seenEnvi[9]), restartIn, key);
      state->envi.soilOrgN = parseDoubleStrict(restartIn, key, value);
      continue;
    }
    if (strcmp(key, "envi.litterN") == 0) {
      markSeen(&(seenEnvi[10]), restartIn, key);
      state->envi.litterN = parseDoubleStrict(restartIn, key, value);
      continue;
    }
    if (strcmp(key, "envi.plantWoodCStorageDelta") == 0) {
      markSeen(&(seenEnvi[11]), restartIn, key);
      state->envi.plantWoodCStorageDelta =
          parseDoubleStrict(restartIn, key, value);
      continue;
    }

    if (strcmp(key, "trackers.gpp") == 0) {
      markSeen(&(seenTrackers[0]), restartIn, key);
      state->trackers.gpp = parseDoubleStrict(restartIn, key, value);
      continue;
    }
    if (strcmp(key, "trackers.rtot") == 0) {
      markSeen(&(seenTrackers[1]), restartIn, key);
      state->trackers.rtot = parseDoubleStrict(restartIn, key, value);
      continue;
    }
    if (strcmp(key, "trackers.ra") == 0) {
      markSeen(&(seenTrackers[2]), restartIn, key);
      state->trackers.ra = parseDoubleStrict(restartIn, key, value);
      continue;
    }
    if (strcmp(key, "trackers.rh") == 0) {
      markSeen(&(seenTrackers[3]), restartIn, key);
      state->trackers.rh = parseDoubleStrict(restartIn, key, value);
      continue;
    }
    if (strcmp(key, "trackers.npp") == 0) {
      markSeen(&(seenTrackers[4]), restartIn, key);
      state->trackers.npp = parseDoubleStrict(restartIn, key, value);
      continue;
    }
    if (strcmp(key, "trackers.nee") == 0) {
      markSeen(&(seenTrackers[5]), restartIn, key);
      state->trackers.nee = parseDoubleStrict(restartIn, key, value);
      continue;
    }
    if (strcmp(key, "trackers.yearlyGpp") == 0) {
      markSeen(&(seenTrackers[6]), restartIn, key);
      state->trackers.yearlyGpp = parseDoubleStrict(restartIn, key, value);
      continue;
    }
    if (strcmp(key, "trackers.yearlyRtot") == 0) {
      markSeen(&(seenTrackers[7]), restartIn, key);
      state->trackers.yearlyRtot = parseDoubleStrict(restartIn, key, value);
      continue;
    }
    if (strcmp(key, "trackers.yearlyRa") == 0) {
      markSeen(&(seenTrackers[8]), restartIn, key);
      state->trackers.yearlyRa = parseDoubleStrict(restartIn, key, value);
      continue;
    }
    if (strcmp(key, "trackers.yearlyRh") == 0) {
      markSeen(&(seenTrackers[9]), restartIn, key);
      state->trackers.yearlyRh = parseDoubleStrict(restartIn, key, value);
      continue;
    }
    if (strcmp(key, "trackers.yearlyNpp") == 0) {
      markSeen(&(seenTrackers[10]), restartIn, key);
      state->trackers.yearlyNpp = parseDoubleStrict(restartIn, key, value);
      continue;
    }
    if (strcmp(key, "trackers.yearlyNee") == 0) {
      markSeen(&(seenTrackers[11]), restartIn, key);
      state->trackers.yearlyNee = parseDoubleStrict(restartIn, key, value);
      continue;
    }
    if (strcmp(key, "trackers.totGpp") == 0) {
      markSeen(&(seenTrackers[12]), restartIn, key);
      state->trackers.totGpp = parseDoubleStrict(restartIn, key, value);
      continue;
    }
    if (strcmp(key, "trackers.totRtot") == 0) {
      markSeen(&(seenTrackers[13]), restartIn, key);
      state->trackers.totRtot = parseDoubleStrict(restartIn, key, value);
      continue;
    }
    if (strcmp(key, "trackers.totRa") == 0) {
      markSeen(&(seenTrackers[14]), restartIn, key);
      state->trackers.totRa = parseDoubleStrict(restartIn, key, value);
      continue;
    }
    if (strcmp(key, "trackers.totRh") == 0) {
      markSeen(&(seenTrackers[15]), restartIn, key);
      state->trackers.totRh = parseDoubleStrict(restartIn, key, value);
      continue;
    }
    if (strcmp(key, "trackers.totNpp") == 0) {
      markSeen(&(seenTrackers[16]), restartIn, key);
      state->trackers.totNpp = parseDoubleStrict(restartIn, key, value);
      continue;
    }
    if (strcmp(key, "trackers.totNee") == 0) {
      markSeen(&(seenTrackers[17]), restartIn, key);
      state->trackers.totNee = parseDoubleStrict(restartIn, key, value);
      continue;
    }
    if (strcmp(key, "trackers.evapotranspiration") == 0) {
      markSeen(&(seenTrackers[18]), restartIn, key);
      state->trackers.evapotranspiration =
          parseDoubleStrict(restartIn, key, value);
      continue;
    }
    if (strcmp(key, "trackers.soilWetnessFrac") == 0) {
      markSeen(&(seenTrackers[19]), restartIn, key);
      state->trackers.soilWetnessFrac =
          parseDoubleStrict(restartIn, key, value);
      continue;
    }
    if (strcmp(key, "trackers.rRoot") == 0) {
      markSeen(&(seenTrackers[20]), restartIn, key);
      state->trackers.rRoot = parseDoubleStrict(restartIn, key, value);
      continue;
    }
    if (strcmp(key, "trackers.rSoil") == 0) {
      markSeen(&(seenTrackers[21]), restartIn, key);
      state->trackers.rSoil = parseDoubleStrict(restartIn, key, value);
      continue;
    }
    if (strcmp(key, "trackers.rAboveground") == 0) {
      markSeen(&(seenTrackers[22]), restartIn, key);
      state->trackers.rAboveground = parseDoubleStrict(restartIn, key, value);
      continue;
    }
    if (strcmp(key, "trackers.yearlyLitter") == 0) {
      markSeen(&(seenTrackers[23]), restartIn, key);
      state->trackers.yearlyLitter = parseDoubleStrict(restartIn, key, value);
      continue;
    }
    if (strcmp(key, "trackers.woodCreation") == 0) {
      markSeen(&(seenTrackers[24]), restartIn, key);
      state->trackers.woodCreation = parseDoubleStrict(restartIn, key, value);
      continue;
    }
    if (strcmp(key, "trackers.n2o") == 0) {
      markSeen(&(seenTrackers[25]), restartIn, key);
      state->trackers.n2o = parseDoubleStrict(restartIn, key, value);
      continue;
    }
    if (strcmp(key, "trackers.gdd") == 0) {
      markSeen(&(seenTrackers[26]), restartIn, key);
      state->trackers.gdd = parseDoubleStrict(restartIn, key, value);
      continue;
    }
    if (strcmp(key, "trackers.lastYear") == 0) {
      markSeen(&(seenTrackers[27]), restartIn, key);
      state->trackers.lastYear = parseIntStrict(restartIn, key, value);
      continue;
    }

    if (strcmp(key, "phenology.didLeafGrowth") == 0) {
      markSeen(&(seenPhenology[0]), restartIn, key);
      state->phenologyTrackers.didLeafGrowth =
          parseIntStrict(restartIn, key, value);
      continue;
    }
    if (strcmp(key, "phenology.didLeafFall") == 0) {
      markSeen(&(seenPhenology[1]), restartIn, key);
      state->phenologyTrackers.didLeafFall =
          parseIntStrict(restartIn, key, value);
      continue;
    }
    if (strcmp(key, "phenology.lastYear") == 0) {
      markSeen(&(seenPhenology[2]), restartIn, key);
      state->phenologyTrackers.lastYear = parseIntStrict(restartIn, key, value);
      continue;
    }

    if (strcmp(key, "event_trackers.d_till_mod") == 0) {
      markSeen(&seenEventTrackers, restartIn, key);
      state->eventTrackers.d_till_mod =
          parseDoubleStrict(restartIn, key, value);
      continue;
    }

    if (strcmp(key, "mean.npp.values.length") == 0) {
      markSeen(&seenMeanValuesLength, restartIn, key);
      meanValuesLength = parseIntStrict(restartIn, key, value);
      if (meanValuesLength < 0) {
        parseValueError(restartIn, key, value);
      }
      seenMeanValues = (int *)calloc((size_t)meanValuesLength, sizeof(int));
      if (seenMeanValues == NULL) {
        parseError(restartIn, "unable to allocate mean values tracker", NULL);
      }
      continue;
    }
    if (strncmp(key, "mean.npp.values.", strlen("mean.npp.values.")) == 0) {
      if (!seenMeanValuesLength) {
        parseError(restartIn,
                   "mean.npp.values.length must appear before "
                   "mean.npp.values.<idx>",
                   key);
      }
      int idx =
          parseIntStrict(restartIn, key, key + strlen("mean.npp.values."));
      if (idx < 0 || idx >= meanValuesLength) {
        parseError(restartIn, "mean.npp.values index out of range", key);
      }
      markSeen(&(seenMeanValues[idx]), restartIn, key);
      meanNPP->values[idx] = parseDoubleStrict(restartIn, key, value);
      continue;
    }

    if (strcmp(key, "mean.npp.weights.length") == 0) {
      markSeen(&seenMeanWeightsLength, restartIn, key);
      meanWeightsLength = parseIntStrict(restartIn, key, value);
      if (meanWeightsLength < 0) {
        parseValueError(restartIn, key, value);
      }
      seenMeanWeights = (int *)calloc((size_t)meanWeightsLength, sizeof(int));
      if (seenMeanWeights == NULL) {
        parseError(restartIn, "unable to allocate mean weights tracker", NULL);
      }
      continue;
    }
    if (strncmp(key, "mean.npp.weights.", strlen("mean.npp.weights.")) == 0) {
      if (!seenMeanWeightsLength) {
        parseError(restartIn,
                   "mean.npp.weights.length must appear before "
                   "mean.npp.weights.<idx>",
                   key);
      }
      int idx =
          parseIntStrict(restartIn, key, key + strlen("mean.npp.weights."));
      if (idx < 0 || idx >= meanWeightsLength) {
        parseError(restartIn, "mean.npp.weights index out of range", key);
      }
      markSeen(&(seenMeanWeights[idx]), restartIn, key);
      meanNPP->weights[idx] = parseDoubleStrict(restartIn, key, value);
      continue;
    }

    if (strcmp(key, "end_restart") == 0) {
      markSeen(&seenEndRestart, restartIn, key);
      if (parseIntStrict(restartIn, key, value) != 1) {
        parseError(restartIn, "end_restart marker must be 1", key);
      }
      continue;
    }

    logError("Restart parse error in %s: unknown key '%s'\n", restartIn, key);
    exit(EXIT_CODE_BAD_PARAMETER_VALUE);
  }

  fclose(in);

  if (!seenModelVersion || !seenBuildInfo || !seenCheckpointUtcEpoch ||
      !seenProcessedSteps) {
    parseError(restartIn, "missing required metadata keys", NULL);
  }
  for (int i = 0; i < 4; ++i) {
    if (!seenSchemaLayout[i]) {
      parseError(restartIn, "missing required schema_layout.* keys", NULL);
    }
  }

  for (int i = 0; i < 10; ++i) {
    if (!seenFlags[i]) {
      parseError(restartIn, "missing required flags.* keys", NULL);
    }
  }
  for (int i = 0; i < 4; ++i) {
    if (!seenBoundary[i]) {
      parseError(restartIn, "missing required boundary.* keys", NULL);
    }
  }
  for (int i = 0; i < 5; ++i) {
    if (!seenMeanMeta[i]) {
      parseError(restartIn, "missing required mean.npp.* keys", NULL);
    }
  }
  for (int i = 0; i < 12; ++i) {
    if (!seenEnvi[i]) {
      parseError(restartIn, "missing required envi.* keys", NULL);
    }
  }
  for (int i = 0; i < 28; ++i) {
    if (!seenTrackers[i]) {
      parseError(restartIn, "missing required trackers.* keys", NULL);
    }
  }
  for (int i = 0; i < 3; ++i) {
    if (!seenPhenology[i]) {
      parseError(restartIn, "missing required phenology.* keys", NULL);
    }
  }
  if (!seenEventTrackers) {
    parseError(restartIn, "missing required event_trackers.* keys", NULL);
  }
  if (!seenMeanValuesLength || !seenMeanWeightsLength) {
    parseError(restartIn, "missing required mean array length keys", NULL);
  }
  if (!seenEndRestart) {
    parseError(restartIn, "missing end_restart marker", NULL);
  }

  if (meanValuesLength != meanNPP->length ||
      meanWeightsLength != meanNPP->length) {
    logError("Restart mean-tracker length mismatch in %s\n", restartIn);
    exit(EXIT_CODE_BAD_PARAMETER_VALUE);
  }

  for (int i = 0; i < meanValuesLength; ++i) {
    if (!seenMeanValues[i]) {
      parseError(restartIn, "mean.npp.values array is incomplete", NULL);
    }
  }
  for (int i = 0; i < meanWeightsLength; ++i) {
    if (!seenMeanWeights[i]) {
      parseError(restartIn, "mean.npp.weights array is incomplete", NULL);
    }
  }

  free(seenMeanValues);
  free(seenMeanWeights);
}

static void writeKeyInt(FILE *out, const char *key, int value) {
  fprintf(out, "%s %d\n", key, value);
}

static void writeKeyLongLong(FILE *out, const char *key, long long value) {
  fprintf(out, "%s %lld\n", key, value);
}

static void writeKeyDouble(FILE *out, const char *key, double value) {
  fprintf(out, "%s %.17g\n", key, value);
}

static void writeRestartState(const char *restartOut,
                              const RestartStateV1 *state,
                              const MeanTracker *meanNPP) {
  FILE *out = openFile(restartOut, "w");

  fprintf(out, "%s %s\n", RESTART_MAGIC, RESTART_SCHEMA_VERSION);
  fprintf(out, "model_version %s\n", state->modelVersion);
  fprintf(out, "build_info %s\n", state->buildInfo);
  writeKeyLongLong(out, "checkpoint_utc_epoch", state->checkpointUtcEpoch);
  writeKeyLongLong(out, "processed_steps", state->processedSteps);
  writeKeyInt(out, "schema_layout.envi_size", RESTART_SCHEMA_LAYOUT_ENVI_SIZE);
  writeKeyInt(out, "schema_layout.trackers_size",
              RESTART_SCHEMA_LAYOUT_TRACKERS_SIZE);
  writeKeyInt(out, "schema_layout.phenology_trackers_size",
              RESTART_SCHEMA_LAYOUT_PHENOLOGY_TRACKERS_SIZE);
  writeKeyInt(out, "schema_layout.event_trackers_size",
              RESTART_SCHEMA_LAYOUT_EVENT_TRACKERS_SIZE);
  fprintf(out, "\n");

  writeKeyInt(out, "flags.events", state->events);
  writeKeyInt(out, "flags.gdd", state->gdd);
  writeKeyInt(out, "flags.growthResp", state->growthResp);
  writeKeyInt(out, "flags.leafWater", state->leafWater);
  writeKeyInt(out, "flags.litterPool", state->litterPool);
  writeKeyInt(out, "flags.snow", state->snow);
  writeKeyInt(out, "flags.soilPhenol", state->soilPhenol);
  writeKeyInt(out, "flags.waterHResp", state->waterHResp);
  writeKeyInt(out, "flags.nitrogenCycle", state->nitrogenCycle);
  writeKeyInt(out, "flags.anaerobic", state->anaerobic);
  fprintf(out, "\n");

  writeKeyInt(out, "boundary.year", state->boundaryClimate.year);
  writeKeyInt(out, "boundary.day", state->boundaryClimate.day);
  writeKeyDouble(out, "boundary.time", state->boundaryClimate.time);
  writeKeyDouble(out, "boundary.length", state->boundaryClimate.length);
  fprintf(out, "\n");

  writeKeyDouble(out, "envi.plantWoodC", state->envi.plantWoodC);
  writeKeyDouble(out, "envi.plantLeafC", state->envi.plantLeafC);
  writeKeyDouble(out, "envi.soilC", state->envi.soilC);
  writeKeyDouble(out, "envi.soilWater", state->envi.soilWater);
  writeKeyDouble(out, "envi.litterC", state->envi.litterC);
  writeKeyDouble(out, "envi.snow", state->envi.snow);
  writeKeyDouble(out, "envi.coarseRootC", state->envi.coarseRootC);
  writeKeyDouble(out, "envi.fineRootC", state->envi.fineRootC);
  writeKeyDouble(out, "envi.minN", state->envi.minN);
  writeKeyDouble(out, "envi.soilOrgN", state->envi.soilOrgN);
  writeKeyDouble(out, "envi.litterN", state->envi.litterN);
  writeKeyDouble(out, "envi.plantWoodCStorageDelta",
                 state->envi.plantWoodCStorageDelta);
  fprintf(out, "\n");

  writeKeyDouble(out, "trackers.gpp", state->trackers.gpp);
  writeKeyDouble(out, "trackers.rtot", state->trackers.rtot);
  writeKeyDouble(out, "trackers.ra", state->trackers.ra);
  writeKeyDouble(out, "trackers.rh", state->trackers.rh);
  writeKeyDouble(out, "trackers.npp", state->trackers.npp);
  writeKeyDouble(out, "trackers.nee", state->trackers.nee);
  writeKeyDouble(out, "trackers.yearlyGpp", state->trackers.yearlyGpp);
  writeKeyDouble(out, "trackers.yearlyRtot", state->trackers.yearlyRtot);
  writeKeyDouble(out, "trackers.yearlyRa", state->trackers.yearlyRa);
  writeKeyDouble(out, "trackers.yearlyRh", state->trackers.yearlyRh);
  writeKeyDouble(out, "trackers.yearlyNpp", state->trackers.yearlyNpp);
  writeKeyDouble(out, "trackers.yearlyNee", state->trackers.yearlyNee);
  writeKeyDouble(out, "trackers.totGpp", state->trackers.totGpp);
  writeKeyDouble(out, "trackers.totRtot", state->trackers.totRtot);
  writeKeyDouble(out, "trackers.totRa", state->trackers.totRa);
  writeKeyDouble(out, "trackers.totRh", state->trackers.totRh);
  writeKeyDouble(out, "trackers.totNpp", state->trackers.totNpp);
  writeKeyDouble(out, "trackers.totNee", state->trackers.totNee);
  writeKeyDouble(out, "trackers.evapotranspiration",
                 state->trackers.evapotranspiration);
  writeKeyDouble(out, "trackers.soilWetnessFrac",
                 state->trackers.soilWetnessFrac);
  writeKeyDouble(out, "trackers.rRoot", state->trackers.rRoot);
  writeKeyDouble(out, "trackers.rSoil", state->trackers.rSoil);
  writeKeyDouble(out, "trackers.rAboveground", state->trackers.rAboveground);
  writeKeyDouble(out, "trackers.yearlyLitter", state->trackers.yearlyLitter);
  writeKeyDouble(out, "trackers.woodCreation", state->trackers.woodCreation);
  writeKeyDouble(out, "trackers.n2o", state->trackers.n2o);
  writeKeyDouble(out, "trackers.gdd", state->trackers.gdd);
  writeKeyInt(out, "trackers.lastYear", state->trackers.lastYear);
  fprintf(out, "\n");

  writeKeyInt(out, "phenology.didLeafGrowth",
              state->phenologyTrackers.didLeafGrowth);
  writeKeyInt(out, "phenology.didLeafFall",
              state->phenologyTrackers.didLeafFall);
  writeKeyInt(out, "phenology.lastYear", state->phenologyTrackers.lastYear);
  fprintf(out, "\n");

  writeKeyDouble(out, "event_trackers.d_till_mod",
                 state->eventTrackers.d_till_mod);
  fprintf(out, "\n");

  writeKeyInt(out, "mean.npp.length", state->meanLength);
  writeKeyDouble(out, "mean.npp.totWeight", state->meanTotWeight);
  writeKeyInt(out, "mean.npp.start", state->meanStart);
  writeKeyInt(out, "mean.npp.last", state->meanLast);
  writeKeyDouble(out, "mean.npp.sum", state->meanSum);
  fprintf(out, "\n");

  writeKeyInt(out, "mean.npp.values.length", meanNPP->length);
  for (int i = 0; i < meanNPP->length; ++i) {
    fprintf(out, "mean.npp.values.%d %.17g\n", i, meanNPP->values[i]);
  }
  fprintf(out, "\n");

  writeKeyInt(out, "mean.npp.weights.length", meanNPP->length);
  for (int i = 0; i < meanNPP->length; ++i) {
    fprintf(out, "mean.npp.weights.%d %.17g\n", i, meanNPP->weights[i]);
  }
  fprintf(out, "\n");

  fprintf(out, "end_restart 1\n");

  fclose(out);
}

static void checkRestartContextCompatibility(const RestartStateV1 *state) {
  int mismatch = 0;

  mismatch |= (ctx.events != state->events);
  mismatch |= (ctx.gdd != state->gdd);
  mismatch |= (ctx.growthResp != state->growthResp);
  mismatch |= (ctx.leafWater != state->leafWater);
  mismatch |= (ctx.litterPool != state->litterPool);
  mismatch |= (ctx.snow != state->snow);
  mismatch |= (ctx.soilPhenol != state->soilPhenol);
  mismatch |= (ctx.waterHResp != state->waterHResp);
  mismatch |= (ctx.nitrogenCycle != state->nitrogenCycle);
  mismatch |= (ctx.anaerobic != state->anaerobic);

  if (mismatch) {
    logError("Restart context mismatch: model flags must match checkpoint "
             "exactly\n");
    exit(EXIT_CODE_BAD_PARAMETER_VALUE);
  }
}

static void validateRestartModelBuild(const RestartStateV1 *state) {
  char currentBuildInfo[sizeof(state->buildInfo)];
  sanitizeBuildInfo(currentBuildInfo, sizeof(currentBuildInfo), VERSION_STRING);

  if (strcmp(state->modelVersion, NUMERIC_VERSION) != 0) {
    logError("Restart model version mismatch: checkpoint=%s current=%s\n",
             state->modelVersion, NUMERIC_VERSION);
    exit(EXIT_CODE_BAD_PARAMETER_VALUE);
  }

  if (strcmp(state->buildInfo, currentBuildInfo) != 0) {
    logWarning("Restart build info mismatch: checkpoint=%s current=%s\n",
               state->buildInfo, currentBuildInfo);
  }
}

static void validateRestartBoundary(const RestartStateV1 *state) {
  if (climate == NULL) {
    logError("Cannot restart: climate forcing has no records\n");
    exit(EXIT_CODE_INPUT_FILE_ERROR);
  }

  if (!climateTimestampIsAfterBoundary(climate, &(state->boundaryClimate))) {
    logError("Restart boundary mismatch: first climate timestamp does not "
             "follow checkpoint boundary timestamp\n");
    logError("Checkpoint boundary: year=%d day=%d time=%.8f\n",
             state->boundaryClimate.year, state->boundaryClimate.day,
             state->boundaryClimate.time);
    logError("Found:    year=%d day=%d time=%.8f\n", climate->year,
             climate->day, climate->time);
    exit(EXIT_CODE_BAD_PARAMETER_VALUE);
  }

  double firstStepHours = climate->length * 24.0;
  if (firstStepHours <= RESTART_FLOAT_EPSILON) {
    logError("Cannot restart: first climate timestep length is non-positive "
             "(year=%d day=%d time=%.8f length=%.8f)\n",
             climate->year, climate->day, climate->time, climate->length);
    exit(EXIT_CODE_BAD_PARAMETER_VALUE);
  }

  int expectedYear = state->boundaryClimate.year;
  int expectedDay = state->boundaryClimate.day;
  advanceOneDay(&expectedYear, &expectedDay);

  if (climate->year != expectedYear || climate->day != expectedDay ||
      climate->time > (firstStepHours + RESTART_FLOAT_EPSILON)) {
    logError("Restart boundary mismatch: resumed segment must start within one "
             "timestep after midnight checkpoint boundary\n");
    logError("Expected start on year=%d day=%d with time<=%.8f; found "
             "year=%d day=%d time=%.8f length=%.8f\n",
             expectedYear, expectedDay, firstStepHours, climate->year,
             climate->day, climate->time, climate->length);
    exit(EXIT_CODE_BAD_PARAMETER_VALUE);
  }
}

static void validateRestartEventBoundary(const RestartStateV1 *state) {
  if (!ctx.events || gEvent == NULL || climate == NULL) {
    return;
  }

  if (dateIsBefore(gEvent->year, gEvent->day, climate->year, climate->day)) {
    logError("Restart event boundary mismatch: first event (year=%d day=%d) is "
             "before resumed climate start (year=%d day=%d)\n",
             gEvent->year, gEvent->day, climate->year, climate->day);
    logError("Checkpoint boundary was year=%d day=%d; event files must be "
             "segmented to the same boundaries as climate segments\n",
             state->boundaryClimate.year, state->boundaryClimate.day);
    exit(EXIT_CODE_BAD_PARAMETER_VALUE);
  }
}

void restartResetRunState(void) {
  processedStepCount = 0;
  hasLastProcessedClimate = 0;
}

void restartNoteProcessedClimateStep(const ClimateNode *climateStep) {
  copyClimateSignature(&lastProcessedClimate, climateStep);
  hasLastProcessedClimate = 1;
  ++processedStepCount;
}

void restartWriteCheckpoint(const char *restartOut,
                            const MeanTracker *meanNPP) {
  if (!hasLastProcessedClimate) {
    logError("Cannot write restart checkpoint %s: no timestep processed\n",
             restartOut);
    exit(EXIT_CODE_BAD_PARAMETER_VALUE);
  }
  validateCheckpointBoundaryForWrite(restartOut, &lastProcessedClimate);

  RestartStateV1 state;
  memset(&state, 0, sizeof(state));

  strncpy(state.modelVersion, NUMERIC_VERSION, sizeof(state.modelVersion) - 1);
  sanitizeBuildInfo(state.buildInfo, sizeof(state.buildInfo), VERSION_STRING);
  state.checkpointUtcEpoch = (long long)time(NULL);
  state.processedSteps = processedStepCount;

  state.events = ctx.events;
  state.gdd = ctx.gdd;
  state.growthResp = ctx.growthResp;
  state.leafWater = ctx.leafWater;
  state.litterPool = ctx.litterPool;
  state.snow = ctx.snow;
  state.soilPhenol = ctx.soilPhenol;
  state.waterHResp = ctx.waterHResp;
  state.nitrogenCycle = ctx.nitrogenCycle;
  state.anaerobic = ctx.anaerobic;

  state.boundaryClimate = lastProcessedClimate;

  state.meanLength = meanNPP->length;
  state.meanTotWeight = meanNPP->totWeight;
  state.meanStart = meanNPP->start;
  state.meanLast = meanNPP->last;
  state.meanSum = meanNPP->sum;

  state.envi = envi;
  state.trackers = trackers;
  state.phenologyTrackers = phenologyTrackers;
  state.eventTrackers = eventTrackers;

  writeRestartState(restartOut, &state, meanNPP);
}

void restartLoadCheckpoint(const char *restartIn, MeanTracker *meanNPP) {
  RestartStateV1 state;
  memset(&state, 0, sizeof(state));

  readRestartState(restartIn, &state, meanNPP);

  validateCheckpointBoundaryForLoad(restartIn, &(state.boundaryClimate));
  checkRestartContextCompatibility(&state);
  validateRestartModelBuild(&state);
  validateRestartBoundary(&state);
  validateRestartEventBoundary(&state);

  if (state.meanStart < 0 || state.meanStart >= meanNPP->length ||
      state.meanLast < 0 || state.meanLast >= meanNPP->length) {
    logError("Restart mean-tracker cursor out of range in %s\n", restartIn);
    exit(EXIT_CODE_BAD_PARAMETER_VALUE);
  }

  envi = state.envi;
  trackers = state.trackers;
  phenologyTrackers = state.phenologyTrackers;
  eventTrackers = state.eventTrackers;

  meanNPP->totWeight = state.meanTotWeight;
  meanNPP->start = state.meanStart;
  meanNPP->last = state.meanLast;
  meanNPP->sum = state.meanSum;

  processedStepCount = state.processedSteps;
  lastProcessedClimate = state.boundaryClimate;
  hasLastProcessedClimate = 1;
}

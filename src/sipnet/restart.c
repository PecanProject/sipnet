#include "restart.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "balance.h"
#include "common/context.h"
#include "common/exitCodes.h"
#include "common/logging.h"
#include "common/util.h"
#include "events.h"
#include "version.h"

#define RESTART_MAGIC "SIPNET_RESTART"
#define RESTART_SCHEMA_VERSION "1.0"
#define RESTART_FLOAT_EPSILON 1e-8

typedef struct RestartClimateSignature {
  int year;
  int day;
  double time;
  double length;
  double tair;
  double tsoil;
  double par;
  double precip;
  double vpd;
  double vpdSoil;
  double vPress;
  double wspd;
  double gdd;
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

  // Event cursor metadata for deterministic replay checks
  int eventCursorIndex;
  int eventCount;
  unsigned long long eventPrefixHash;
  int eventHasNext;
  unsigned long long eventNextHash;

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
  BalanceTracker balanceTracker;
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
  dest->tair = src->tair;
  dest->tsoil = src->tsoil;
  dest->par = src->par;
  dest->precip = src->precip;
  dest->vpd = src->vpd;
  dest->vpdSoil = src->vpdSoil;
  dest->vPress = src->vPress;
  dest->wspd = src->wspd;
  dest->gdd = src->gdd;
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
  long long parsed = strtoll(value, &end, 10);
  if (end == value || *end != '\0') {
    parseValueError(restartIn, key, value);
  }
  return parsed;
}

static int parseIntStrict(const char *restartIn, const char *key,
                          const char *value) {
  long long parsed = parseLongLongStrict(restartIn, key, value);
  return (int)parsed;
}

static unsigned long long parseUnsignedLongLongStrict(const char *restartIn,
                                                      const char *key,
                                                      const char *value) {
  char *end = NULL;
  unsigned long long parsed = strtoull(value, &end, 10);
  if (end == value || *end != '\0') {
    parseValueError(restartIn, key, value);
  }
  return parsed;
}

static double parseDoubleStrict(const char *restartIn, const char *key,
                                const char *value) {
  char *end = NULL;
  double parsed = strtod(value, &end);
  if (end == value || *end != '\0') {
    parseValueError(restartIn, key, value);
  }
  return parsed;
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

  int seenFlags[10] = {0};
  int seenBoundary[13] = {0};
  int seenEventState[5] = {0};
  int seenMeanMeta[5] = {0};
  int seenEnvi[12] = {0};
  int seenTrackers[27] = {0};
  int seenPhenology[3] = {0};
  int seenEventTrackers = 0;
  int seenBalance[10] = {0};

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
    if (strcmp(key, "boundary.tair") == 0) {
      markSeen(&(seenBoundary[4]), restartIn, key);
      state->boundaryClimate.tair = parseDoubleStrict(restartIn, key, value);
      continue;
    }
    if (strcmp(key, "boundary.tsoil") == 0) {
      markSeen(&(seenBoundary[5]), restartIn, key);
      state->boundaryClimate.tsoil = parseDoubleStrict(restartIn, key, value);
      continue;
    }
    if (strcmp(key, "boundary.par") == 0) {
      markSeen(&(seenBoundary[6]), restartIn, key);
      state->boundaryClimate.par = parseDoubleStrict(restartIn, key, value);
      continue;
    }
    if (strcmp(key, "boundary.precip") == 0) {
      markSeen(&(seenBoundary[7]), restartIn, key);
      state->boundaryClimate.precip = parseDoubleStrict(restartIn, key, value);
      continue;
    }
    if (strcmp(key, "boundary.vpd") == 0) {
      markSeen(&(seenBoundary[8]), restartIn, key);
      state->boundaryClimate.vpd = parseDoubleStrict(restartIn, key, value);
      continue;
    }
    if (strcmp(key, "boundary.vpdSoil") == 0) {
      markSeen(&(seenBoundary[9]), restartIn, key);
      state->boundaryClimate.vpdSoil = parseDoubleStrict(restartIn, key, value);
      continue;
    }
    if (strcmp(key, "boundary.vPress") == 0) {
      markSeen(&(seenBoundary[10]), restartIn, key);
      state->boundaryClimate.vPress = parseDoubleStrict(restartIn, key, value);
      continue;
    }
    if (strcmp(key, "boundary.wspd") == 0) {
      markSeen(&(seenBoundary[11]), restartIn, key);
      state->boundaryClimate.wspd = parseDoubleStrict(restartIn, key, value);
      continue;
    }
    if (strcmp(key, "boundary.gdd") == 0) {
      markSeen(&(seenBoundary[12]), restartIn, key);
      state->boundaryClimate.gdd = parseDoubleStrict(restartIn, key, value);
      continue;
    }

    if (strcmp(key, "event_state.cursor_index") == 0) {
      markSeen(&(seenEventState[0]), restartIn, key);
      state->eventCursorIndex = parseIntStrict(restartIn, key, value);
      continue;
    }
    if (strcmp(key, "event_state.event_count") == 0) {
      markSeen(&(seenEventState[1]), restartIn, key);
      state->eventCount = parseIntStrict(restartIn, key, value);
      continue;
    }
    if (strcmp(key, "event_state.prefix_hash") == 0) {
      markSeen(&(seenEventState[2]), restartIn, key);
      state->eventPrefixHash =
          parseUnsignedLongLongStrict(restartIn, key, value);
      continue;
    }
    if (strcmp(key, "event_state.has_next") == 0) {
      markSeen(&(seenEventState[3]), restartIn, key);
      state->eventHasNext = parseIntStrict(restartIn, key, value);
      continue;
    }
    if (strcmp(key, "event_state.next_hash") == 0) {
      markSeen(&(seenEventState[4]), restartIn, key);
      state->eventNextHash = parseUnsignedLongLongStrict(restartIn, key, value);
      continue;
    }

    if (strcmp(key, "mean.length") == 0) {
      markSeen(&(seenMeanMeta[0]), restartIn, key);
      state->meanLength = parseIntStrict(restartIn, key, value);
      continue;
    }
    if (strcmp(key, "mean.totWeight") == 0) {
      markSeen(&(seenMeanMeta[1]), restartIn, key);
      state->meanTotWeight = parseDoubleStrict(restartIn, key, value);
      continue;
    }
    if (strcmp(key, "mean.start") == 0) {
      markSeen(&(seenMeanMeta[2]), restartIn, key);
      state->meanStart = parseIntStrict(restartIn, key, value);
      continue;
    }
    if (strcmp(key, "mean.last") == 0) {
      markSeen(&(seenMeanMeta[3]), restartIn, key);
      state->meanLast = parseIntStrict(restartIn, key, value);
      continue;
    }
    if (strcmp(key, "mean.sum") == 0) {
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
    if (strcmp(key, "trackers.lastYear") == 0) {
      markSeen(&(seenTrackers[26]), restartIn, key);
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

    if (strcmp(key, "balance.preTotalC") == 0) {
      markSeen(&(seenBalance[0]), restartIn, key);
      state->balanceTracker.preTotalC =
          parseDoubleStrict(restartIn, key, value);
      continue;
    }
    if (strcmp(key, "balance.postTotalC") == 0) {
      markSeen(&(seenBalance[1]), restartIn, key);
      state->balanceTracker.postTotalC =
          parseDoubleStrict(restartIn, key, value);
      continue;
    }
    if (strcmp(key, "balance.inputsC") == 0) {
      markSeen(&(seenBalance[2]), restartIn, key);
      state->balanceTracker.inputsC = parseDoubleStrict(restartIn, key, value);
      continue;
    }
    if (strcmp(key, "balance.outputsC") == 0) {
      markSeen(&(seenBalance[3]), restartIn, key);
      state->balanceTracker.outputsC = parseDoubleStrict(restartIn, key, value);
      continue;
    }
    if (strcmp(key, "balance.preTotalN") == 0) {
      markSeen(&(seenBalance[4]), restartIn, key);
      state->balanceTracker.preTotalN =
          parseDoubleStrict(restartIn, key, value);
      continue;
    }
    if (strcmp(key, "balance.postTotalN") == 0) {
      markSeen(&(seenBalance[5]), restartIn, key);
      state->balanceTracker.postTotalN =
          parseDoubleStrict(restartIn, key, value);
      continue;
    }
    if (strcmp(key, "balance.inputsN") == 0) {
      markSeen(&(seenBalance[6]), restartIn, key);
      state->balanceTracker.inputsN = parseDoubleStrict(restartIn, key, value);
      continue;
    }
    if (strcmp(key, "balance.outputsN") == 0) {
      markSeen(&(seenBalance[7]), restartIn, key);
      state->balanceTracker.outputsN = parseDoubleStrict(restartIn, key, value);
      continue;
    }
    if (strcmp(key, "balance.deltaC") == 0) {
      markSeen(&(seenBalance[8]), restartIn, key);
      state->balanceTracker.deltaC = parseDoubleStrict(restartIn, key, value);
      continue;
    }
    if (strcmp(key, "balance.deltaN") == 0) {
      markSeen(&(seenBalance[9]), restartIn, key);
      state->balanceTracker.deltaN = parseDoubleStrict(restartIn, key, value);
      continue;
    }

    if (strcmp(key, "mean.values.length") == 0) {
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
    if (strncmp(key, "mean.values.", strlen("mean.values.")) == 0) {
      if (!seenMeanValuesLength) {
        parseError(restartIn,
                   "mean.values.length must appear before mean.values.<idx>",
                   key);
      }
      int idx = parseIntStrict(restartIn, key, key + strlen("mean.values."));
      if (idx < 0 || idx >= meanValuesLength) {
        parseError(restartIn, "mean.values index out of range", key);
      }
      markSeen(&(seenMeanValues[idx]), restartIn, key);
      meanNPP->values[idx] = parseDoubleStrict(restartIn, key, value);
      continue;
    }

    if (strcmp(key, "mean.weights.length") == 0) {
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
    if (strncmp(key, "mean.weights.", strlen("mean.weights.")) == 0) {
      if (!seenMeanWeightsLength) {
        parseError(restartIn,
                   "mean.weights.length must appear before mean.weights.<idx>",
                   key);
      }
      int idx = parseIntStrict(restartIn, key, key + strlen("mean.weights."));
      if (idx < 0 || idx >= meanWeightsLength) {
        parseError(restartIn, "mean.weights index out of range", key);
      }
      markSeen(&(seenMeanWeights[idx]), restartIn, key);
      meanNPP->weights[idx] = parseDoubleStrict(restartIn, key, value);
      continue;
    }

    if (strcmp(key, "end_restart") == 0) {
      markSeen(&seenEndRestart, restartIn, key);
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

  for (int i = 0; i < 10; ++i) {
    if (!seenFlags[i]) {
      parseError(restartIn, "missing required flags.* keys", NULL);
    }
  }
  for (int i = 0; i < 13; ++i) {
    if (!seenBoundary[i]) {
      parseError(restartIn, "missing required boundary.* keys", NULL);
    }
  }
  for (int i = 0; i < 5; ++i) {
    if (!seenEventState[i]) {
      parseError(restartIn, "missing required event_state.* keys", NULL);
    }
  }
  for (int i = 0; i < 5; ++i) {
    if (!seenMeanMeta[i]) {
      parseError(restartIn, "missing required mean.* keys", NULL);
    }
  }
  for (int i = 0; i < 12; ++i) {
    if (!seenEnvi[i]) {
      parseError(restartIn, "missing required envi.* keys", NULL);
    }
  }
  for (int i = 0; i < 27; ++i) {
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
  for (int i = 0; i < 10; ++i) {
    if (!seenBalance[i]) {
      parseError(restartIn, "missing required balance.* keys", NULL);
    }
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
      parseError(restartIn, "mean.values array is incomplete", NULL);
    }
  }
  for (int i = 0; i < meanWeightsLength; ++i) {
    if (!seenMeanWeights[i]) {
      parseError(restartIn, "mean.weights array is incomplete", NULL);
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

static void writeKeyUnsignedLongLong(FILE *out, const char *key,
                                     unsigned long long value) {
  fprintf(out, "%s %llu\n", key, value);
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
  writeKeyDouble(out, "boundary.tair", state->boundaryClimate.tair);
  writeKeyDouble(out, "boundary.tsoil", state->boundaryClimate.tsoil);
  writeKeyDouble(out, "boundary.par", state->boundaryClimate.par);
  writeKeyDouble(out, "boundary.precip", state->boundaryClimate.precip);
  writeKeyDouble(out, "boundary.vpd", state->boundaryClimate.vpd);
  writeKeyDouble(out, "boundary.vpdSoil", state->boundaryClimate.vpdSoil);
  writeKeyDouble(out, "boundary.vPress", state->boundaryClimate.vPress);
  writeKeyDouble(out, "boundary.wspd", state->boundaryClimate.wspd);
  writeKeyDouble(out, "boundary.gdd", state->boundaryClimate.gdd);
  fprintf(out, "\n");

  writeKeyInt(out, "event_state.cursor_index", state->eventCursorIndex);
  writeKeyInt(out, "event_state.event_count", state->eventCount);
  writeKeyUnsignedLongLong(out, "event_state.prefix_hash",
                           state->eventPrefixHash);
  writeKeyInt(out, "event_state.has_next", state->eventHasNext);
  writeKeyUnsignedLongLong(out, "event_state.next_hash", state->eventNextHash);
  fprintf(out, "\n");

  writeKeyInt(out, "mean.length", state->meanLength);
  writeKeyDouble(out, "mean.totWeight", state->meanTotWeight);
  writeKeyInt(out, "mean.start", state->meanStart);
  writeKeyInt(out, "mean.last", state->meanLast);
  writeKeyDouble(out, "mean.sum", state->meanSum);
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

  writeKeyDouble(out, "balance.preTotalC", state->balanceTracker.preTotalC);
  writeKeyDouble(out, "balance.postTotalC", state->balanceTracker.postTotalC);
  writeKeyDouble(out, "balance.inputsC", state->balanceTracker.inputsC);
  writeKeyDouble(out, "balance.outputsC", state->balanceTracker.outputsC);
  writeKeyDouble(out, "balance.preTotalN", state->balanceTracker.preTotalN);
  writeKeyDouble(out, "balance.postTotalN", state->balanceTracker.postTotalN);
  writeKeyDouble(out, "balance.inputsN", state->balanceTracker.inputsN);
  writeKeyDouble(out, "balance.outputsN", state->balanceTracker.outputsN);
  writeKeyDouble(out, "balance.deltaC", state->balanceTracker.deltaC);
  writeKeyDouble(out, "balance.deltaN", state->balanceTracker.deltaN);
  fprintf(out, "\n");

  writeKeyInt(out, "mean.values.length", meanNPP->length);
  for (int i = 0; i < meanNPP->length; ++i) {
    fprintf(out, "mean.values.%d %.17g\n", i, meanNPP->values[i]);
  }
  fprintf(out, "\n");

  writeKeyInt(out, "mean.weights.length", meanNPP->length);
  for (int i = 0; i < meanNPP->length; ++i) {
    fprintf(out, "mean.weights.%d %.17g\n", i, meanNPP->weights[i]);
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
}

static void applyRestartGddCarry(const RestartStateV1 *state) {
  if (!ctx.gdd || climate == NULL) {
    return;
  }

  double firstStepGdd = climate->tair * climate->length;
  if (firstStepGdd < 0) {
    firstStepGdd = 0;
  }

  double expectedFirstGdd = firstStepGdd;
  if (climate->year == state->boundaryClimate.year) {
    expectedFirstGdd = state->boundaryClimate.gdd + firstStepGdd;
  }

  double gddOffset = expectedFirstGdd - climate->gdd;
  ClimateNode *curr = climate;
  while (curr != NULL && curr->year == climate->year) {
    curr->gdd += gddOffset;
    curr = curr->nextClim;
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

  if (ctx.events) {
    state.eventCursorIndex = getEventCursorIndex();
    if (state.eventCursorIndex < 0) {
      logError("Internal error: invalid event cursor while writing restart\n");
      exit(EXIT_CODE_INTERNAL_ERROR);
    }
    state.eventCount = getEventCount();
    state.eventPrefixHash = getEventPrefixHash(state.eventCursorIndex);
    state.eventNextHash =
        getEventHashAtIndex(state.eventCursorIndex, &(state.eventHasNext));
  } else {
    state.eventCursorIndex = 0;
    state.eventCount = 0;
    state.eventPrefixHash = 0ULL;
    state.eventHasNext = 0;
    state.eventNextHash = 0ULL;
  }

  state.meanLength = meanNPP->length;
  state.meanTotWeight = meanNPP->totWeight;
  state.meanStart = meanNPP->start;
  state.meanLast = meanNPP->last;
  state.meanSum = meanNPP->sum;

  state.envi = envi;
  state.trackers = trackers;
  state.phenologyTrackers = phenologyTrackers;
  state.eventTrackers = eventTrackers;
  state.balanceTracker = balanceTracker;

  writeRestartState(restartOut, &state, meanNPP);
}

void restartLoadCheckpoint(const char *restartIn, MeanTracker *meanNPP) {
  RestartStateV1 state;
  memset(&state, 0, sizeof(state));

  readRestartState(restartIn, &state, meanNPP);

  checkRestartContextCompatibility(&state);
  validateRestartModelBuild(&state);
  validateRestartBoundary(&state);
  applyRestartGddCarry(&state);

  if (state.meanStart < 0 || state.meanStart >= meanNPP->length ||
      state.meanLast < 0 || state.meanLast >= meanNPP->length) {
    logError("Restart mean-tracker cursor out of range in %s\n", restartIn);
    exit(EXIT_CODE_BAD_PARAMETER_VALUE);
  }

  envi = state.envi;
  trackers = state.trackers;
  phenologyTrackers = state.phenologyTrackers;
  eventTrackers = state.eventTrackers;
  balanceTracker = state.balanceTracker;

  meanNPP->totWeight = state.meanTotWeight;
  meanNPP->start = state.meanStart;
  meanNPP->last = state.meanLast;
  meanNPP->sum = state.meanSum;

  processedStepCount = state.processedSteps;
  lastProcessedClimate = state.boundaryClimate;
  hasLastProcessedClimate = 1;

  if (ctx.events) {
    if (state.eventCount != getEventCount()) {
      logError("Restart events mismatch: event count changed (%d vs %d)\n",
               state.eventCount, getEventCount());
      exit(EXIT_CODE_BAD_PARAMETER_VALUE);
    }
    if (state.eventCursorIndex < 0 ||
        state.eventCursorIndex > state.eventCount) {
      logError("Restart events mismatch: invalid cursor index %d\n",
               state.eventCursorIndex);
      exit(EXIT_CODE_BAD_PARAMETER_VALUE);
    }

    unsigned long long prefixHash = getEventPrefixHash(state.eventCursorIndex);
    if (prefixHash != state.eventPrefixHash) {
      logError("Restart events mismatch: processed event history changed\n");
      exit(EXIT_CODE_BAD_PARAMETER_VALUE);
    }

    int hasNext = 0;
    unsigned long long nextHash =
        getEventHashAtIndex(state.eventCursorIndex, &hasNext);
    if ((hasNext != state.eventHasNext) ||
        (hasNext && nextHash != state.eventNextHash)) {
      logError(
          "Restart events mismatch: next event cursor is not deterministic\n");
      exit(EXIT_CODE_BAD_PARAMETER_VALUE);
    }

    if (setEventCursorIndex(state.eventCursorIndex) != 0) {
      logError("Restart events mismatch: unable to set event cursor to %d\n",
               state.eventCursorIndex);
      exit(EXIT_CODE_BAD_PARAMETER_VALUE);
    }
  }
}

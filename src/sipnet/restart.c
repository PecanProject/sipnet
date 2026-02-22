#include "restart.h"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "balance.h"
#include "common/context.h"
#include "common/exitCodes.h"
#include "common/logging.h"
#include "common/util.h"
#include "events.h"
#include "version.h"

#define RESTART_MAGIC "SIPNET_RESTART_V1"
#define RESTART_SCHEMA_VERSION 1

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

typedef struct RestartHeaderV1 {
  char magic[32];
  int schemaVersion;
  char modelVersion[32];
  char buildInfo[96];

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
} RestartHeaderV1;

typedef struct RestartPayloadV1 {
  Envi envi;
  Trackers trackers;
  PhenologyTrackers phenologyTrackers;
  EventTrackers eventTrackers;
  BalanceTracker balanceTracker;
} RestartPayloadV1;

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

static int restartDoublesMatch(double a, double b) {
  return fabs(a - b) < 1e-10;
}

static int climateSignaturesMatch(const ClimateNode *actual,
                                  const RestartClimateSignature *expected) {
  int mismatch = 0;
  mismatch |= (actual->year != expected->year);
  mismatch |= (actual->day != expected->day);
  mismatch |= !restartDoublesMatch(actual->time, expected->time);
  mismatch |= !restartDoublesMatch(actual->length, expected->length);
  mismatch |= !restartDoublesMatch(actual->tair, expected->tair);
  mismatch |= !restartDoublesMatch(actual->tsoil, expected->tsoil);
  mismatch |= !restartDoublesMatch(actual->par, expected->par);
  mismatch |= !restartDoublesMatch(actual->precip, expected->precip);
  mismatch |= !restartDoublesMatch(actual->vpd, expected->vpd);
  mismatch |= !restartDoublesMatch(actual->vpdSoil, expected->vpdSoil);
  mismatch |= !restartDoublesMatch(actual->vPress, expected->vPress);
  mismatch |= !restartDoublesMatch(actual->wspd, expected->wspd);
  return !mismatch;
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

static void checkRestartContextCompatibility(const RestartHeaderV1 *header) {
  int mismatch = 0;

  mismatch |= (ctx.events != header->events);
  mismatch |= (ctx.gdd != header->gdd);
  mismatch |= (ctx.growthResp != header->growthResp);
  mismatch |= (ctx.leafWater != header->leafWater);
  mismatch |= (ctx.litterPool != header->litterPool);
  mismatch |= (ctx.snow != header->snow);
  mismatch |= (ctx.soilPhenol != header->soilPhenol);
  mismatch |= (ctx.waterHResp != header->waterHResp);
  mismatch |= (ctx.nitrogenCycle != header->nitrogenCycle);
  mismatch |= (ctx.anaerobic != header->anaerobic);

  if (mismatch) {
    logError("Restart context mismatch: model flags must match checkpoint "
             "exactly\n");
    exit(EXIT_CODE_BAD_PARAMETER_VALUE);
  }
}

static void validateRestartBoundary(const RestartHeaderV1 *header) {
  if (climate == NULL) {
    logError("Cannot restart: climate forcing has no records\n");
    exit(EXIT_CODE_INPUT_FILE_ERROR);
  }

  const RestartClimateSignature *expected = &(header->boundaryClimate);
  if (!climateSignaturesMatch(climate, expected)) {
    logError("Restart boundary mismatch: first climate row does not match "
             "checkpoint metadata\n");
    logError("Expected: year=%d day=%d time=%.8f\n", expected->year,
             expected->day, expected->time);
    logError("Found:    year=%d day=%d time=%.8f\n", climate->year,
             climate->day, climate->time);
    exit(EXIT_CODE_BAD_PARAMETER_VALUE);
  }
}

static void validateRestartModelBuild(const RestartHeaderV1 *header) {
  char currentBuildInfo[sizeof(header->buildInfo)];
  sanitizeBuildInfo(currentBuildInfo, sizeof(currentBuildInfo), VERSION_STRING);

  if (strcmp(header->modelVersion, NUMERIC_VERSION) != 0) {
    logError("Restart model version mismatch: checkpoint=%s current=%s\n",
             header->modelVersion, NUMERIC_VERSION);
    exit(EXIT_CODE_BAD_PARAMETER_VALUE);
  }

  if (strcmp(header->buildInfo, currentBuildInfo) != 0) {
    logError("Restart build info mismatch: checkpoint=%s current=%s\n",
             header->buildInfo, currentBuildInfo);
    exit(EXIT_CODE_BAD_PARAMETER_VALUE);
  }
}

static void applyRestartGddCarry(const RestartHeaderV1 *header) {
  if (!ctx.gdd || climate == NULL) {
    return;
  }

  if (climate->year != header->boundaryClimate.year) {
    return;
  }

  double gddOffset = header->boundaryClimate.gdd - climate->gdd;
  ClimateNode *curr = climate;
  while (curr != NULL && curr->year == header->boundaryClimate.year) {
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

  FILE *out = openFile(restartOut, "wb");

  RestartHeaderV1 header;
  memset(&header, 0, sizeof(header));

  strncpy(header.magic, RESTART_MAGIC, sizeof(header.magic) - 1);
  header.schemaVersion = RESTART_SCHEMA_VERSION;
  strncpy(header.modelVersion, NUMERIC_VERSION,
          sizeof(header.modelVersion) - 1);
  sanitizeBuildInfo(header.buildInfo, sizeof(header.buildInfo), VERSION_STRING);

  header.processedSteps = processedStepCount;

  header.events = ctx.events;
  header.gdd = ctx.gdd;
  header.growthResp = ctx.growthResp;
  header.leafWater = ctx.leafWater;
  header.litterPool = ctx.litterPool;
  header.snow = ctx.snow;
  header.soilPhenol = ctx.soilPhenol;
  header.waterHResp = ctx.waterHResp;
  header.nitrogenCycle = ctx.nitrogenCycle;
  header.anaerobic = ctx.anaerobic;

  header.boundaryClimate = lastProcessedClimate;

  if (ctx.events) {
    header.eventCursorIndex = getEventCursorIndex();
    if (header.eventCursorIndex < 0) {
      logError("Internal error: invalid event cursor while writing restart\n");
      exit(EXIT_CODE_INTERNAL_ERROR);
    }
    header.eventCount = getEventCount();
    header.eventPrefixHash = getEventPrefixHash(header.eventCursorIndex);
    header.eventNextHash =
        getEventHashAtIndex(header.eventCursorIndex, &(header.eventHasNext));
  } else {
    header.eventCursorIndex = 0;
    header.eventCount = 0;
    header.eventPrefixHash = 0ULL;
    header.eventHasNext = 0;
    header.eventNextHash = 0ULL;
  }

  header.meanLength = meanNPP->length;
  header.meanTotWeight = meanNPP->totWeight;
  header.meanStart = meanNPP->start;
  header.meanLast = meanNPP->last;
  header.meanSum = meanNPP->sum;

  RestartPayloadV1 payload;
  payload.envi = envi;
  payload.trackers = trackers;
  payload.phenologyTrackers = phenologyTrackers;
  payload.eventTrackers = eventTrackers;
  payload.balanceTracker = balanceTracker;

  if (fwrite(&header, sizeof(header), 1, out) != 1) {
    logError("Error writing restart header to %s\n", restartOut);
    exit(EXIT_CODE_FILE_OPEN_OR_READ_ERROR);
  }
  if (fwrite(&payload, sizeof(payload), 1, out) != 1) {
    logError("Error writing restart payload to %s\n", restartOut);
    exit(EXIT_CODE_FILE_OPEN_OR_READ_ERROR);
  }
  if (fwrite(meanNPP->values, sizeof(double), meanNPP->length, out) !=
      (size_t)meanNPP->length) {
    logError("Error writing restart mean values to %s\n", restartOut);
    exit(EXIT_CODE_FILE_OPEN_OR_READ_ERROR);
  }
  if (fwrite(meanNPP->weights, sizeof(double), meanNPP->length, out) !=
      (size_t)meanNPP->length) {
    logError("Error writing restart mean weights to %s\n", restartOut);
    exit(EXIT_CODE_FILE_OPEN_OR_READ_ERROR);
  }

  fclose(out);
}

void restartLoadCheckpoint(const char *restartIn, MeanTracker *meanNPP) {
  FILE *in = openFile(restartIn, "rb");

  RestartHeaderV1 header;
  RestartPayloadV1 payload;
  memset(&header, 0, sizeof(header));
  memset(&payload, 0, sizeof(payload));

  if (fread(&header, sizeof(header), 1, in) != 1) {
    logError("Error reading restart header from %s\n", restartIn);
    exit(EXIT_CODE_FILE_OPEN_OR_READ_ERROR);
  }

  if (strncmp(header.magic, RESTART_MAGIC, strlen(RESTART_MAGIC)) != 0) {
    logError("Restart file %s has invalid magic header\n", restartIn);
    exit(EXIT_CODE_BAD_PARAMETER_VALUE);
  }
  if (header.schemaVersion != RESTART_SCHEMA_VERSION) {
    logError("Restart schema mismatch in %s: found %d expected %d\n", restartIn,
             header.schemaVersion, RESTART_SCHEMA_VERSION);
    exit(EXIT_CODE_BAD_PARAMETER_VALUE);
  }

  checkRestartContextCompatibility(&header);
  validateRestartModelBuild(&header);
  validateRestartBoundary(&header);
  applyRestartGddCarry(&header);

  if (fread(&payload, sizeof(payload), 1, in) != 1) {
    logError("Error reading restart payload from %s\n", restartIn);
    exit(EXIT_CODE_FILE_OPEN_OR_READ_ERROR);
  }

  if (header.meanLength != meanNPP->length) {
    logError("Restart mean-tracker length mismatch in %s\n", restartIn);
    exit(EXIT_CODE_BAD_PARAMETER_VALUE);
  }

  if (fread(meanNPP->values, sizeof(double), meanNPP->length, in) !=
      (size_t)meanNPP->length) {
    logError("Error reading restart mean values from %s\n", restartIn);
    exit(EXIT_CODE_FILE_OPEN_OR_READ_ERROR);
  }
  if (fread(meanNPP->weights, sizeof(double), meanNPP->length, in) !=
      (size_t)meanNPP->length) {
    logError("Error reading restart mean weights from %s\n", restartIn);
    exit(EXIT_CODE_FILE_OPEN_OR_READ_ERROR);
  }

  fclose(in);

  if (header.meanStart < 0 || header.meanStart >= meanNPP->length ||
      header.meanLast < 0 || header.meanLast >= meanNPP->length) {
    logError("Restart mean-tracker cursor out of range in %s\n", restartIn);
    exit(EXIT_CODE_BAD_PARAMETER_VALUE);
  }

  envi = payload.envi;
  trackers = payload.trackers;
  phenologyTrackers = payload.phenologyTrackers;
  eventTrackers = payload.eventTrackers;
  balanceTracker = payload.balanceTracker;

  meanNPP->totWeight = header.meanTotWeight;
  meanNPP->start = header.meanStart;
  meanNPP->last = header.meanLast;
  meanNPP->sum = header.meanSum;

  processedStepCount = header.processedSteps;
  lastProcessedClimate = header.boundaryClimate;
  hasLastProcessedClimate = 1;

  if (ctx.events) {
    if (header.eventCount != getEventCount()) {
      logError("Restart events mismatch: event count changed (%d vs %d)\n",
               header.eventCount, getEventCount());
      exit(EXIT_CODE_BAD_PARAMETER_VALUE);
    }
    if (header.eventCursorIndex < 0 ||
        header.eventCursorIndex > header.eventCount) {
      logError("Restart events mismatch: invalid cursor index %d\n",
               header.eventCursorIndex);
      exit(EXIT_CODE_BAD_PARAMETER_VALUE);
    }

    unsigned long long prefixHash = getEventPrefixHash(header.eventCursorIndex);
    if (prefixHash != header.eventPrefixHash) {
      logError("Restart events mismatch: processed event history changed\n");
      exit(EXIT_CODE_BAD_PARAMETER_VALUE);
    }

    int hasNext = 0;
    unsigned long long nextHash =
        getEventHashAtIndex(header.eventCursorIndex, &hasNext);
    if ((hasNext != header.eventHasNext) ||
        (hasNext && nextHash != header.eventNextHash)) {
      logError(
          "Restart events mismatch: next event cursor is not deterministic\n");
      exit(EXIT_CODE_BAD_PARAMETER_VALUE);
    }

    if (setEventCursorIndex(header.eventCursorIndex) != 0) {
      logError("Restart events mismatch: unable to set event cursor to %d\n",
               header.eventCursorIndex);
      exit(EXIT_CODE_BAD_PARAMETER_VALUE);
    }
  }

  // Boundary row is already accounted for in checkpoint state.
  if (climate != NULL) {
    climate = climate->nextClim;
  }
}

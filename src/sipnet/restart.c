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

#define RESTART_MAGIC "SIPNET_RESTART_ASCII"
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

static int restartDoublesMatch(double a, double b) {
  return fabs(a - b) <= RESTART_FLOAT_EPSILON;
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

static void readTag(FILE *in, const char *restartIn, const char *expectedTag) {
  char tag[64];
  if (fscanf(in, "%63s", tag) != 1) {
    logError("Restart file %s ended unexpectedly while reading tag '%s'\n",
             restartIn, expectedTag);
    exit(EXIT_CODE_FILE_OPEN_OR_READ_ERROR);
  }
  if (strcmp(tag, expectedTag) != 0) {
    logError("Restart parse error in %s: expected tag '%s', found '%s'\n",
             restartIn, expectedTag, tag);
    exit(EXIT_CODE_BAD_PARAMETER_VALUE);
  }
}

static void readClimateSignature(FILE *in, const char *restartIn,
                                 RestartClimateSignature *sig) {
  int status = fscanf(in, "%d %d %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf",
                      &(sig->year), &(sig->day), &(sig->time), &(sig->length),
                      &(sig->tair), &(sig->tsoil), &(sig->par), &(sig->precip),
                      &(sig->vpd), &(sig->vpdSoil), &(sig->vPress),
                      &(sig->wspd), &(sig->gdd));
  if (status != 13) {
    logError("Restart parse error in %s: invalid boundary climate signature\n",
             restartIn);
    exit(EXIT_CODE_FILE_OPEN_OR_READ_ERROR);
  }
}

static void writeClimateSignature(FILE *out,
                                  const RestartClimateSignature *sig) {
  fprintf(
      out,
      "%d %d %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g",
      sig->year, sig->day, sig->time, sig->length, sig->tair, sig->tsoil,
      sig->par, sig->precip, sig->vpd, sig->vpdSoil, sig->vPress, sig->wspd,
      sig->gdd);
}

static void readRestartState(const char *restartIn, RestartStateV1 *state,
                             MeanTracker *meanNPP) {
  FILE *in = openFile(restartIn, "r");

  char magic[64];
  char schemaVersion[16];
  if (fscanf(in, "%63s %15s", magic, schemaVersion) != 2) {
    logError("Error reading restart header from %s\n", restartIn);
    exit(EXIT_CODE_FILE_OPEN_OR_READ_ERROR);
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

  readTag(in, restartIn, "model_version");
  if (fscanf(in, "%31s", state->modelVersion) != 1) {
    logError("Restart parse error in %s: model_version missing\n", restartIn);
    exit(EXIT_CODE_FILE_OPEN_OR_READ_ERROR);
  }

  readTag(in, restartIn, "build_info");
  if (fscanf(in, "%95s", state->buildInfo) != 1) {
    logError("Restart parse error in %s: build_info missing\n", restartIn);
    exit(EXIT_CODE_FILE_OPEN_OR_READ_ERROR);
  }

  readTag(in, restartIn, "checkpoint_utc_epoch");
  if (fscanf(in, "%lld", &(state->checkpointUtcEpoch)) != 1) {
    logError("Restart parse error in %s: checkpoint_utc_epoch missing\n",
             restartIn);
    exit(EXIT_CODE_FILE_OPEN_OR_READ_ERROR);
  }

  readTag(in, restartIn, "processed_steps");
  if (fscanf(in, "%lld", &(state->processedSteps)) != 1) {
    logError("Restart parse error in %s: processed_steps missing\n", restartIn);
    exit(EXIT_CODE_FILE_OPEN_OR_READ_ERROR);
  }

  readTag(in, restartIn, "flags");
  if (fscanf(in, "%d %d %d %d %d %d %d %d %d %d", &(state->events),
             &(state->gdd), &(state->growthResp), &(state->leafWater),
             &(state->litterPool), &(state->snow), &(state->soilPhenol),
             &(state->waterHResp), &(state->nitrogenCycle),
             &(state->anaerobic)) != 10) {
    logError("Restart parse error in %s: invalid flags section\n", restartIn);
    exit(EXIT_CODE_FILE_OPEN_OR_READ_ERROR);
  }

  readTag(in, restartIn, "boundary");
  readClimateSignature(in, restartIn, &(state->boundaryClimate));

  readTag(in, restartIn, "event_state");
  if (fscanf(in, "%d %d %llu %d %llu", &(state->eventCursorIndex),
             &(state->eventCount), &(state->eventPrefixHash),
             &(state->eventHasNext), &(state->eventNextHash)) != 5) {
    logError("Restart parse error in %s: invalid event_state section\n",
             restartIn);
    exit(EXIT_CODE_FILE_OPEN_OR_READ_ERROR);
  }

  readTag(in, restartIn, "mean_meta");
  if (fscanf(in, "%d %lf %d %d %lf", &(state->meanLength),
             &(state->meanTotWeight), &(state->meanStart), &(state->meanLast),
             &(state->meanSum)) != 5) {
    logError("Restart parse error in %s: invalid mean_meta section\n",
             restartIn);
    exit(EXIT_CODE_FILE_OPEN_OR_READ_ERROR);
  }

  readTag(in, restartIn, "envi");
  if (fscanf(in, "%lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf",
             &(state->envi.plantWoodC), &(state->envi.plantLeafC),
             &(state->envi.soilC), &(state->envi.soilWater),
             &(state->envi.litterC), &(state->envi.snow),
             &(state->envi.coarseRootC), &(state->envi.fineRootC),
             &(state->envi.minN), &(state->envi.soilOrgN),
             &(state->envi.litterN),
             &(state->envi.plantWoodCStorageDelta)) != 12) {
    logError("Restart parse error in %s: invalid envi section\n", restartIn);
    exit(EXIT_CODE_FILE_OPEN_OR_READ_ERROR);
  }

  readTag(in, restartIn, "trackers");
  if (fscanf(in,
             "%lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf "
             "%lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %d",
             &(state->trackers.gpp), &(state->trackers.rtot),
             &(state->trackers.ra), &(state->trackers.rh),
             &(state->trackers.npp), &(state->trackers.nee),
             &(state->trackers.yearlyGpp), &(state->trackers.yearlyRtot),
             &(state->trackers.yearlyRa), &(state->trackers.yearlyRh),
             &(state->trackers.yearlyNpp), &(state->trackers.yearlyNee),
             &(state->trackers.totGpp), &(state->trackers.totRtot),
             &(state->trackers.totRa), &(state->trackers.totRh),
             &(state->trackers.totNpp), &(state->trackers.totNee),
             &(state->trackers.evapotranspiration),
             &(state->trackers.soilWetnessFrac), &(state->trackers.rRoot),
             &(state->trackers.rSoil), &(state->trackers.rAboveground),
             &(state->trackers.yearlyLitter), &(state->trackers.woodCreation),
             &(state->trackers.n2o), &(state->trackers.lastYear)) != 27) {
    logError("Restart parse error in %s: invalid trackers section\n",
             restartIn);
    exit(EXIT_CODE_FILE_OPEN_OR_READ_ERROR);
  }

  readTag(in, restartIn, "phenology");
  if (fscanf(in, "%d %d %d", &(state->phenologyTrackers.didLeafGrowth),
             &(state->phenologyTrackers.didLeafFall),
             &(state->phenologyTrackers.lastYear)) != 3) {
    logError("Restart parse error in %s: invalid phenology section\n",
             restartIn);
    exit(EXIT_CODE_FILE_OPEN_OR_READ_ERROR);
  }

  readTag(in, restartIn, "event_trackers");
  if (fscanf(in, "%lf", &(state->eventTrackers.d_till_mod)) != 1) {
    logError("Restart parse error in %s: invalid event_trackers section\n",
             restartIn);
    exit(EXIT_CODE_FILE_OPEN_OR_READ_ERROR);
  }

  readTag(in, restartIn, "balance");
  if (fscanf(
          in, "%lf %lf %lf %lf %lf %lf %lf %lf %lf %lf",
          &(state->balanceTracker.preTotalC),
          &(state->balanceTracker.postTotalC), &(state->balanceTracker.inputsC),
          &(state->balanceTracker.outputsC), &(state->balanceTracker.preTotalN),
          &(state->balanceTracker.postTotalN), &(state->balanceTracker.inputsN),
          &(state->balanceTracker.outputsN), &(state->balanceTracker.deltaC),
          &(state->balanceTracker.deltaN)) != 10) {
    logError("Restart parse error in %s: invalid balance section\n", restartIn);
    exit(EXIT_CODE_FILE_OPEN_OR_READ_ERROR);
  }

  readTag(in, restartIn, "mean_values");
  int meanValuesLen = 0;
  if (fscanf(in, "%d", &meanValuesLen) != 1) {
    logError("Restart parse error in %s: mean_values length missing\n",
             restartIn);
    exit(EXIT_CODE_FILE_OPEN_OR_READ_ERROR);
  }
  if (meanValuesLen != meanNPP->length) {
    logError("Restart mean-tracker length mismatch in %s\n", restartIn);
    exit(EXIT_CODE_BAD_PARAMETER_VALUE);
  }
  for (int i = 0; i < meanValuesLen; ++i) {
    if (fscanf(in, "%lf", &(meanNPP->values[i])) != 1) {
      logError("Restart parse error in %s: mean_values truncated\n", restartIn);
      exit(EXIT_CODE_FILE_OPEN_OR_READ_ERROR);
    }
  }

  readTag(in, restartIn, "mean_weights");
  int meanWeightsLen = 0;
  if (fscanf(in, "%d", &meanWeightsLen) != 1) {
    logError("Restart parse error in %s: mean_weights length missing\n",
             restartIn);
    exit(EXIT_CODE_FILE_OPEN_OR_READ_ERROR);
  }
  if (meanWeightsLen != meanNPP->length) {
    logError("Restart mean-tracker length mismatch in %s\n", restartIn);
    exit(EXIT_CODE_BAD_PARAMETER_VALUE);
  }
  for (int i = 0; i < meanWeightsLen; ++i) {
    if (fscanf(in, "%lf", &(meanNPP->weights[i])) != 1) {
      logError("Restart parse error in %s: mean_weights truncated\n",
               restartIn);
      exit(EXIT_CODE_FILE_OPEN_OR_READ_ERROR);
    }
  }

  readTag(in, restartIn, "end_restart");

  fclose(in);
}

static void writeRestartState(const char *restartOut,
                              const RestartStateV1 *state,
                              const MeanTracker *meanNPP) {
  FILE *out = openFile(restartOut, "w");

  fprintf(out, "%s %s\n", RESTART_MAGIC, RESTART_SCHEMA_VERSION);
  fprintf(out, "model_version %s\n", state->modelVersion);
  fprintf(out, "build_info %s\n", state->buildInfo);
  fprintf(out, "checkpoint_utc_epoch %lld\n", state->checkpointUtcEpoch);
  fprintf(out, "processed_steps %lld\n", state->processedSteps);
  fprintf(out, "flags %d %d %d %d %d %d %d %d %d %d\n", state->events,
          state->gdd, state->growthResp, state->leafWater, state->litterPool,
          state->snow, state->soilPhenol, state->waterHResp,
          state->nitrogenCycle, state->anaerobic);

  fprintf(out, "boundary ");
  writeClimateSignature(out, &(state->boundaryClimate));
  fprintf(out, "\n");

  fprintf(out, "event_state %d %d %llu %d %llu\n", state->eventCursorIndex,
          state->eventCount, state->eventPrefixHash, state->eventHasNext,
          state->eventNextHash);

  fprintf(out, "mean_meta %d %.17g %d %d %.17g\n", state->meanLength,
          state->meanTotWeight, state->meanStart, state->meanLast,
          state->meanSum);

  fprintf(out,
          "envi %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g "
          "%.17g %.17g\n",
          state->envi.plantWoodC, state->envi.plantLeafC, state->envi.soilC,
          state->envi.soilWater, state->envi.litterC, state->envi.snow,
          state->envi.coarseRootC, state->envi.fineRootC, state->envi.minN,
          state->envi.soilOrgN, state->envi.litterN,
          state->envi.plantWoodCStorageDelta);

  fprintf(
      out,
      "trackers %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g "
      "%.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g "
      "%.17g %.17g %.17g %.17g %.17g %d\n",
      state->trackers.gpp, state->trackers.rtot, state->trackers.ra,
      state->trackers.rh, state->trackers.npp, state->trackers.nee,
      state->trackers.yearlyGpp, state->trackers.yearlyRtot,
      state->trackers.yearlyRa, state->trackers.yearlyRh,
      state->trackers.yearlyNpp, state->trackers.yearlyNee,
      state->trackers.totGpp, state->trackers.totRtot, state->trackers.totRa,
      state->trackers.totRh, state->trackers.totNpp, state->trackers.totNee,
      state->trackers.evapotranspiration, state->trackers.soilWetnessFrac,
      state->trackers.rRoot, state->trackers.rSoil,
      state->trackers.rAboveground, state->trackers.yearlyLitter,
      state->trackers.woodCreation, state->trackers.n2o,
      state->trackers.lastYear);

  fprintf(out, "phenology %d %d %d\n", state->phenologyTrackers.didLeafGrowth,
          state->phenologyTrackers.didLeafFall,
          state->phenologyTrackers.lastYear);

  fprintf(out, "event_trackers %.17g\n", state->eventTrackers.d_till_mod);

  fprintf(
      out,
      "balance %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g\n",
      state->balanceTracker.preTotalC, state->balanceTracker.postTotalC,
      state->balanceTracker.inputsC, state->balanceTracker.outputsC,
      state->balanceTracker.preTotalN, state->balanceTracker.postTotalN,
      state->balanceTracker.inputsN, state->balanceTracker.outputsN,
      state->balanceTracker.deltaC, state->balanceTracker.deltaN);

  fprintf(out, "mean_values %d", meanNPP->length);
  for (int i = 0; i < meanNPP->length; ++i) {
    fprintf(out, " %.17g", meanNPP->values[i]);
  }
  fprintf(out, "\n");

  fprintf(out, "mean_weights %d", meanNPP->length);
  for (int i = 0; i < meanNPP->length; ++i) {
    fprintf(out, " %.17g", meanNPP->weights[i]);
  }
  fprintf(out, "\n");

  fprintf(out, "end_restart\n");

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
    logError("Restart build info mismatch: checkpoint=%s current=%s\n",
             state->buildInfo, currentBuildInfo);
    exit(EXIT_CODE_BAD_PARAMETER_VALUE);
  }
}

static void validateRestartBoundary(const RestartStateV1 *state) {
  if (climate == NULL) {
    logError("Cannot restart: climate forcing has no records\n");
    exit(EXIT_CODE_INPUT_FILE_ERROR);
  }

  if (!climateSignaturesMatch(climate, &(state->boundaryClimate))) {
    logError("Restart boundary mismatch: first climate row does not match "
             "checkpoint metadata\n");
    logError("Expected: year=%d day=%d time=%.8f\n",
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

  if (climate->year != state->boundaryClimate.year) {
    return;
  }

  double gddOffset = state->boundaryClimate.gdd - climate->gdd;
  ClimateNode *curr = climate;
  while (curr != NULL && curr->year == state->boundaryClimate.year) {
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

  // Boundary row is already accounted for in checkpoint state.
  if (climate != NULL) {
    climate = climate->nextClim;
  }
}

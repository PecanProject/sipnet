#ifndef SIPNET_RESTART_H
#define SIPNET_RESTART_H

#include "runmean.h"
#include "state.h"

void restartResetRunState(void);

void restartNoteProcessedClimateStep(const ClimateNode *climateStep);

void restartWriteCheckpoint(const char *restartOut, const MeanTracker *meanNPP);

void restartLoadCheckpoint(const char *restartIn, MeanTracker *meanNPP);

#endif  // SIPNET_RESTART_H

// header file for per-timestep SIPNET debug logging of envi, fluxes, and
// trackers state

#ifndef DEBUG_LOG_H
#define DEBUG_LOG_H

#include <stdio.h>

typedef struct DebugLogFiles {
  FILE *envi;
  FILE *fluxes;
  FILE *trackers;
} DebugLogFiles;

void initDebugArrays(void);
void initDebugLogFiles(DebugLogFiles *debugLogFiles);
void openDebugLogFiles(DebugLogFiles *debugLogFiles,
                       const char *debugLogPrefix);
void closeDebugLogFiles(DebugLogFiles *debugLogFiles);
void freeDebugArrays(void);
void outputDebugHeaders(DebugLogFiles *debugLogFiles);
void outputDebugState(DebugLogFiles *debugLogFiles, int year, int day,
                      double time);

#endif

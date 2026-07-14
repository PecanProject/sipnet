// header file for debug_log.c

#ifndef DEBUG_LOG_H
#define DEBUG_LOG_H

#include <stdio.h>

typedef struct DebugOutputFiles {
  FILE *envi;
  FILE *fluxes;
  FILE *trackers;
} DebugOutputFiles;

void initDebugOutputFiles(DebugOutputFiles *debugOutputFiles);
void openDebugOutputFiles(DebugOutputFiles *debugOutputFiles,
                          const char *debugOutputPrefix);
void closeDebugOutputFiles(DebugOutputFiles *debugOutputFiles);
void outputDebugHeaders(DebugOutputFiles *debugOutputFiles);
void outputDebugState(DebugOutputFiles *debugOutputFiles, int year, int day,
                      double time);

#endif

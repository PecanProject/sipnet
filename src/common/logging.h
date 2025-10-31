#ifndef SIPNET_LOGGING_H
#define SIPNET_LOGGING_H

#include <stdio.h>
#include <stdarg.h>

#include "context.h"

#ifdef __FILE_NAME__
#define FILE_NAME __FILE_NAME__
#else
#define FILE_NAME __FILE__  // Pretty verbose, but it will have to do
#endif
/*!
 * Someday, this might grow into a real logging system
 *
 * For now, it is just some convenience wrappers around printf that has access
 * to the Context
 *
 * Also, logging levels:
 * 0: silenced when --quiet
 * 1: not silenced by --quiet
 * 2: as 1, but also prints file and line number in output
 */
// clang-format off
#define logAppend(...)  logprint(0, "",           FILE_NAME, __LINE__, __VA_ARGS__)
#define logWarning(...) logprint(0, "[WARNING] ", FILE_NAME, __LINE__, __VA_ARGS__)
#define logInfo(...)    logprint(0, "[INFO   ] ", FILE_NAME, __LINE__, __VA_ARGS__)
#define logTest(...)    logprint(1, "[TEST   ] ", FILE_NAME, __LINE__, __VA_ARGS__)
#define logError(...)   logprint(1, "[ERROR  ] ", FILE_NAME, __LINE__, __VA_ARGS__)
#define logInternalError(...) logprint(2, "[ERROR (INTERNAL)] ", FILE_NAME, __LINE__, __VA_ARGS__)
// clang-format on

void logprint(int logLevel, const char *prefix, const char *file, int lineNum,
              const char *fmt, ...);

#endif  // SIPNET_LOGGING_H

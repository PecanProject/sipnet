#ifndef SIPNET_LOGGING_H
#define SIPNET_LOGGING_H

#include <stdio.h>
#include <stdarg.h>

#include "context.h"

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
#define logWarning(...) logprint(0, "[WARNING] ", __FILE_NAME__, __LINE__, __VA_ARGS__)
#define logInfo(...)    logprint(0, "[INFO   ] ", __FILE_NAME__, __LINE__, __VA_ARGS__)
#define logTest(...)    logprint(1, "[TEST   ] ", __FILE_NAME__, __LINE__, __VA_ARGS__)
#define logError(...)   logprint(1, "[ERROR  ] ", __FILE_NAME__, __LINE__, __VA_ARGS__)
#define logInternalError(...) logprint(2, "[ERROR (INTERNAL)] ", __FILE_NAME__, __LINE__, __VA_ARGS__)
// clang-format on

void logprint(int logLevel, const char *prefix, const char *file, int lineNum,
              const char *fmt, ...);

#endif  // SIPNET_LOGGING_H

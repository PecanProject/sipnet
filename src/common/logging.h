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
 */

#define logWarning(...) logprint(1, "[WARNING] ", __VA_ARGS__)
#define logInfo(...) logprint(1, "[INFO   ] ", __VA_ARGS__)
#define logError(...) logprint(0, "[ERROR  ] ", __VA_ARGS__)

void logprint(int checkContext, const char *prefix, const char *fmt, ...);

#endif  // SIPNET_LOGGING_H

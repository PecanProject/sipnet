#include "logging.h"

void logprint(int checkContext, const char *prefix, const char *fmt, ...) {
  if (checkContext && !ctx.quiet) {
    va_list args;
    printf("%s", prefix);
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
  }
}

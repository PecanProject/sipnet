#include "logging.h"
#include "exitCodes.h"

void logprint(int logLevel, const char *prefix, const char *file, int lineNum,
              const char *fmt, ...) {
  if (logLevel == 0 && ctx.quiet) {
    return;
  }
  switch (logLevel) {
    case 0:
    case 1:
      printf("%s", prefix);
      break;
    case 2:
      printf("%s (%s:%d) ", prefix, file, lineNum);
      break;
    default:
      // paranoia check
      // We should probably call logError here, but, on the very off chance we
      // get here, that feels like a potential infinite loop
      printf(
          "[ERROR (INTERNAL)] (%s:%d) Unknown error level %d while trying to "
          "print output; original output from (%s:%d)\n",
          FILE_NAME, __LINE__, logLevel, file, lineNum);
      exit(EXIT_CODE_INTERNAL_ERROR);
  }
  va_list args;
  va_start(args, fmt);
  vprintf(fmt, args);
  va_end(args);
}

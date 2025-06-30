#include "logging.h"
#include "exitCodes.h"

// Note: errVal here isn't scalable, but we will replace it if/when we have
// a proper logging system
void logprint(int errLevel, const char *prefix, const char *file, int lineNum,
              const char *fmt, ...) {
  if (errLevel == 0 && ctx.quiet) {
    return;
  }
  switch (errLevel) {
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
          __FILE_NAME__, __LINE__, errLevel, file, lineNum);
      exit(EXIT_CODE_INTERNAL_ERROR);
  }
  va_list args;
  va_start(args, fmt);
  vprintf(fmt, args);
  va_end(args);
}

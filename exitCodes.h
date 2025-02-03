#ifndef EXITCODES_H
#define EXITCODES_H

// Exit codes for SIPNET; most of the current uses of exit(1) should
// get better codes, which will appear here
typedef enum {
  EXIT_SUCCESS = 0,
  EXIT_FAILURE = 1,
  EXIT_UNKNOWN_EVENT = 2
} ExitCode;

#endif

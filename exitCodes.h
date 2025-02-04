#ifndef EXITCODES_H
#define EXITCODES_H

// Exit codes for SIPNET; most of the current uses of exit(1) should
// get better codes, which will appear here
typedef enum {
  EXIT_CODE_SUCCESS = 0,
  EXIT_CODE_FAILURE = 1,
  EXIT_CODE_UNKNOWN_EVENT = 2
} exit_code_t;

#endif

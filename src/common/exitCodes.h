#ifndef EXITCODES_H
#define EXITCODES_H

// Exit codes for SIPNET; most of the current uses of exit(1) should
// get better codes, which will appear here.
//
// Note for future: some codes typically have other meanings and should not be
// used here (except for 0 and 1, for which our meaning is the usual meaning).
// Some somewhat-standard codes in use:
// - 0-2, 126-165, and 255; see, e.g.,
// https://tldp.org/LDP/abs/html/exitcodes.html
// - 64-78, see https://man7.org/linux/man-pages/man3/sysexits.h.3head.html
//
// The upshot here is that we should be a little sparing in our list of error
// codes.
typedef enum {
  EXIT_CODE_SUCCESS = 0,
  EXIT_CODE_FAILURE = 1,  // generic failure
  // code 2 typically has a special meaning, see above
  EXIT_CODE_BAD_PARAMETER_VALUE = 3,
  EXIT_CODE_UNKNOWN_EVENT_TYPE_OR_PARAM = 4,
  EXIT_CODE_INPUT_FILE_ERROR = 5,
  EXIT_CODE_FILE_OPEN_ERROR = 6,
} exit_code_t;

#endif

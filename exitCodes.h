#ifndef EXITCODES_H
#define EXITCODES_H

// Exit codes for SIPNET; most of the current uses of exit(1) should
// get better codes, which will appear here
typedef enum {
  EXIT_CODE_SUCCESS = 0,
  EXIT_CODE_FAILURE = 1, // generic failure
  // code 2 typically has a special meaning, see below
  EXIT_CODE_UNKNOWN_EVENT = 3
} exit_code_t;

// Note for future: these codes typically have other meanings and should not be
// used here (except for 0 and 1, for which our meaning is the special meaning)
// 0-2, 126-165, and 255
// See, e.g., https://tldp.org/LDP/abs/html/exitcodes.html

#endif

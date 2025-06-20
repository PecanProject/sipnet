#include <stdio.h>
#include <setjmp.h>
#include <unistd.h>

#include "common/logging.h"

static int expected_code = 1;  // the expected value a tested function passes to
                               // exit
static int should_exit = 1;  // set in test code; 1 if exit should have been
                             // called
static int really_exit = 0;  // set to 1 to prevent stubbing behavior and
                             // actually exit

static jmp_buf jump_env;

static int exit_result = 1;
#define test_assert(x) (exit_result = exit_result && (x))

// set should_exit=0 when code SHOULDN'T exit(), e.g.:
//
// should_exit = 0;
// if (!(jmp_rval=setjmp(jump_env)))
// {
//   call_to_non_exiting_code();
// }
// test_assert(jmp_rval==0);
//
// set should_exit=1 when exit is expected:
//
// should_exit = 1;
// expected_code = 1; // or whatever the exit code should be
// if (!(jmp_rval=setjmp(jump_env)))
// {
//   call_to_exiting_code();
// }
//
// test_assert(jmp_rval==1);

// stub function
void exit(int code) {
  if (!really_exit) {
    logTest("Mocking the call to exit()\n");
    test_assert(should_exit == 1);
    test_assert(expected_code == code);
    longjmp(jump_env, 1);
  }

  _exit(code);
}

#include <stdio.h>

#include "utils/tUtils.h"
#include "utils/exitHandler.c"
#include "sipnet/events.c"

// These test points check the fix for issue
// https://github.com/PecanProject/sipnet/issues/74

int run(void) {
  int status = 0;

  // Set up the context
  initContext();
  updateIntContext("events", 1, CTX_TEST);

  // exit() handling params
  int jmp_rval;

  // First test - this should pass, not exit
  should_exit = 0;
  exit_result = 1;
  expected_code = EXIT_CODE_INPUT_FILE_ERROR;
  jmp_rval = setjmp(jump_env);
  if (!jmp_rval) {
    readEventData("infra_events_year_boundary.in");
  }
  test_assert(jmp_rval == 0);
  status |= !exit_result;
  if (!exit_result) {
    logTest("FAIL with infra_events_year_boundary.in\n");
  }

  // Second test - this should exit
  should_exit = 1;
  exit_result = 1;  // reset for next test
  expected_code = EXIT_CODE_INPUT_FILE_ERROR;
  jmp_rval = setjmp(jump_env);
  if (!jmp_rval) {
    readEventData("infra_events_bad_order.in");
  }
  test_assert(jmp_rval == 1);
  status |= !exit_result;
  if (!exit_result) {
    logTest("FAIL with infra_events_bad_order.in\n");
  }

  // Allow a real exit, not that this is really needed
  really_exit = 1;

  return status;
}

int main(void) {
  int status;

  logTest("Starting testEventFileOrderChecks:run()\n");
  status = run();
  if (status) {
    really_exit = 1;
    logTest("FAILED testEventFileOrderChecks with status %d\n", status);
    exit(status);
  }

  logTest("PASSED testEventFileOrderChecks\n");
}

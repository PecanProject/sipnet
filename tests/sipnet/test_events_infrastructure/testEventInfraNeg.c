
#include <stdio.h>

#include "sipnet/events.c"
#include "common/logging.h"
#include "utils/exitHandler.c"

int run(void) {
  int status = 0;

  // exit() handling params
  int jmp_rval;

  // Step 0: make sure that we don't have a false positive on good data
  should_exit = 0;
  exit_result = 1;
  jmp_rval = setjmp(jump_env);
  if (!jmp_rval) {
    readEventData("infra_events_simple.in");
  }
  test_assert(jmp_rval == 0);
  status |= !exit_result;
  if (status) {
    logTest("FAIL with infra_events_simple.in");
  }

  // Remaining tests should exit()
  should_exit = 1;

  // First test
  exit_result = 1;  // reset for next test
  expected_code = EXIT_CODE_UNKNOWN_EVENT_TYPE_OR_PARAM;
  jmp_rval = setjmp(jump_env);
  if (!jmp_rval) {
    readEventData("infra_events_unknown.in");
  }
  test_assert(jmp_rval == 1);
  status |= !exit_result;
  if (!exit_result) {
    logTest("FAIL with infra_events_unknown.in\n");
  }

  // Second test
  exit_result = 1;  // reset for next test
  expected_code = EXIT_CODE_INPUT_FILE_ERROR;
  jmp_rval = setjmp(jump_env);
  if (!jmp_rval) {
    readEventData("infra_events_date_ooo.in");
  }
  test_assert(jmp_rval == 1);
  status |= !exit_result;
  if (!exit_result) {
    logTest("FAIL with infra_events_date_ooo.in\n");
  }

  // Bad data tests - NAs in various places
  // Year NA, first line
  exit_result = 1;  // reset for next test
  expected_code = EXIT_CODE_INPUT_FILE_ERROR;
  jmp_rval = setjmp(jump_env);
  if (!jmp_rval) {
    readEventData("infra_events_bad_first.in");
  }
  test_assert(jmp_rval == 1);
  status |= !exit_result;
  if (!exit_result) {
    logTest("FAIL with infra_events_bad_first.in\n");
  }

  // NA in a till event
  exit_result = 1;  // reset for next test
  expected_code = EXIT_CODE_INPUT_FILE_ERROR;
  jmp_rval = setjmp(jump_env);
  if (!jmp_rval) {
    readEventData("infra_events_bad_till.in");
  }
  test_assert(jmp_rval == 1);
  status |= !exit_result;
  if (!exit_result) {
    logTest("FAIL with infra_events_bad_till.in\n");
  }

  // Allow a real exit, not that this is really needed
  really_exit = 1;

  return status;
}

int main(void) {
  int status;

  logTest("Starting testEventInfraNeg:run()\n");
  status = run();
  if (status) {
    really_exit = 1;
    logTest("FAILED testEventInfraNeg with status %d\n", status);
    exit(status);
  }

  logTest("PASSED testEventInfraNeg\n");
}


#include <stdio.h>
#include <stdlib.h>

#include "modelStructures.h"
#include "../../../events.c"
#include "../exitHandler.c"

int run() {
  int status = 0;

  // exit() handling params
  int jmp_rval;
  expected_code = 1;
  exit_result = 1;

  // Step 0: make sure that we don't have a false positive on good data
  should_exit = 0;
  jmp_rval = setjmp(jump_env);
  if (!jmp_rval) {
    readEventData("infra_events_simple.in", 1);
  }
  test_assert(jmp_rval==0);
  status |= !exit_result;

  // First test
  should_exit = 1;
  jmp_rval = setjmp(jump_env);
  if (!jmp_rval) {
    readEventData("infra_events_unknown.in", 1);
  }
  test_assert(jmp_rval==1);
  status |= !exit_result;

  // Second test
  jmp_rval = setjmp(jump_env);
  if (!jmp_rval) {
    readEventData("infra_events_loc_ooo.in", 1);
  }
  test_assert(jmp_rval==1);
  status |= !exit_result;

  // Third test
  jmp_rval = setjmp(jump_env);
  if (!jmp_rval) {
    readEventData("infra_events_date_ooo.in", 1);
  }
  test_assert(jmp_rval==1);
  status |= !exit_result;

  // Allow a real exit, not that this is really needed
  really_exit = 1;

  return status;
}

int main() {
  int status;

  printf("Starting testEventInfraNeg:run()\n");
  status = run();
  if (status) {
    printf("Test run failed with status %d\n", status);
    exit(status);
  }

  printf("testEventInfra PASSED\n");
}
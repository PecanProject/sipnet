#include <stdio.h>

#include "modelStructures.h"  //NOLINT
#include "sipnet/events.c"
#include "utils/exitHandler.c"
#include "sipnet/sipnet.c"

void initEnv(void) {
  envi.plantLeafC = 1;
  envi.plantWoodC = 2;
  envi.fineRootC = 3;
  envi.coarseRootC = 4;
}

int run(void) {
  int status = 0;
  int numLocs = 1;

  // set up dummy climate
  climate = malloc(numLocs * sizeof(climate));
  climate->year = 2024;
  climate->day = 70;

  // set up dummy envi/fluxes/params
  params.leafCSpWt = 5.0;
  params.leafAllocation = 0;  // Should trigger error
  params.woodAllocation = 0.5;
  params.fineRootAllocation = 0.1;
  ensureAllocation();

  // init values
  initEnv();

  // exit() handling params
  int jmp_rval;
  should_exit = 1;

  // First test
  exit_result = 1;  // reset for next test
  expected_code = EXIT_CODE_BAD_PARAMETER_VALUE;
  jmp_rval = setjmp(jump_env);
  if (!jmp_rval) {
    events = readEventData("events_one_planting.in", numLocs);
    setupEvents(0);
    processEvents();
  }
  test_assert(jmp_rval == 1);
  status |= !exit_result;
  if (!exit_result) {
    printf("FAIL in bad leafAllocation param test\n");
  }

  return status;
}

int main(void) {
  int status;

  printf("Starting testEventTypeNeg:run()\n");
  status = run();
  if (status) {
    really_exit = 1;
    printf("FAILED testEventTypeNeg with status %d\n", status);
    exit(status);
  }

  printf("PASSED testEventTypeNeg\n");
}

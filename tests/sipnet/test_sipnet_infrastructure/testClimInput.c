#include <stdio.h>

#include "utils/tUtils.h"
#include "utils/exitHandler.c"
#include "sipnet/sipnet.c"

int runTest(const char *climFile) {
  int numRecords = 0;
  int expNumRecords = 10;

  // Read climate data from file
  readClimData(climFile);

  // Make sure correct number of records are read
  ClimateNode *curr = firstClimate;
  while (curr != NULL) {
    ++numRecords;
    curr = curr->nextClim;
  }

  freeClimateList();

  return numRecords != expNumRecords;
}

int run() {
  int status = 0;
  // exit() handling params
  int jmp_rval;

  // Step 0: all positive tests to run
  really_exit = 1;

  // First test - standard read
  status |= runTest("standard.clim");

  // Second test - legacy read (clim file with location column)
  status |= runTest("with_loc.clim");

  // Third test - multi location (sbould error)
  really_exit = 0;
  should_exit = 1;
  exit_result = 1;  // reset for next test
  expected_code = EXIT_CODE_INPUT_FILE_ERROR;
  jmp_rval = setjmp(jump_env);
  if (!jmp_rval) {
    runTest("multi_loc.clim");
  }
  test_assert(jmp_rval == 1);
  status |= !exit_result;
  if (!exit_result) {
    printf("FAIL with multi_loc.clim\n");
  }

  return status;
}

int main() {
  int status;

  status = run();
  if (status) {
    printf("FAILED testClimInput with status %d\n", status);
    exit(status);
  }

  printf("PASSED testClimInput\n");
}

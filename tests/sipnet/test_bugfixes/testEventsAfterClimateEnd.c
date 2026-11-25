#include "utils/tUtils.h"
#include "utils/exitHandler.c"
#include "sipnet/events.c"
#include "sipnet/sipnet.c"

static int runCase(int validEvent, int pendingYear, int pendingDay) {
  initContext();
  updateIntContext("events", 1, CTX_TEST);
  ModelParams *modelParams;
  initModel(&modelParams, "../test_system/balance.param",
            "../test_system/balance.clim");
  // Use only the first climate record (2016, day 46).
  ClimateNode *remainingClimate = firstClimate->nextClim;
  firstClimate->nextClim = NULL;
  gEvents = validEvent ? createEventNode(2016, 46, IRRIGATION, "1.0 1") : NULL;
  EventNode *pending = pendingYear ? createEventNode(pendingYear, pendingDay,
                                                     IRRIGATION, "1.0 1")
                                   : NULL;
  if (gEvents) {
    gEvents->nextEvent = pending;
  } else {
    gEvents = pending;
  }
  eventOutFile = tmpfile();
  if (!eventOutFile) {
    really_exit = 1;
    exit(1);
  }

  should_exit = pendingYear != 0;
  expected_code = EXIT_CODE_INPUT_FILE_ERROR;
  exit_result = 1;
  int jumped = setjmp(jump_env);
  if (!jumped) {
    runModelOutput(NULL, NULL, NULL, 0);
  }
  test_assert(jumped == should_exit);
  test_assert(getCurrentEvent() == pending);

  // A valid event on the final day must still be applied exactly once.
  rewind(eventOutFile);
  char line[1024];
  int rows = 0;
  while (fgets(line, sizeof(line), eventOutFile)) {
    test_assert(strstr(line, "eventSoilWater=1.00") != NULL);
    rows++;
  }
  test_assert(rows == validEvent);
  int status = !exit_result;
  logTest("Final climate event case: valid=%d pending=%d/%d %s\n", validEvent,
          pendingYear, pendingDay, status ? "FAIL" : "PASS");
  firstClimate->nextClim = remainingClimate;
  cleanupModel();
  deleteModelParams(modelParams);
  gEvents = gEvent = NULL;
  return status;
}

int main(void) {
  int status = runCase(0, 0, 0);
  status |= runCase(1, 0, 0);
  status |= runCase(0, 2016, 47);
  status |= runCase(0, 2017, 1);
  status |= runCase(1, 2016, 47);
  status |= runCase(1, 2017, 1);
  logTest("%s testEventsAfterClimateEnd\n", status ? "FAILED" : "PASSED");
  return status;
}

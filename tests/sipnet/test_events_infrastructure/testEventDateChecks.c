#include "utils/tUtils.h"
#include "common/context.h"
#include "sipnet/events.c"
#include "utils/exitHandler.c"

static ClimateNode testClimate;

static void resetTest(void) {
  freeEventList();
  gEvents = NULL;
  gEvent = NULL;
  fluxes = (Fluxes){0};
  initEventTrackers();
  testClimate = (ClimateNode){.year = 2016, .day = 100, .length = 1.0};
  climate = &testClimate;
}

static int testMissedEvent(int year, int day, int currentYear, int currentDay) {
  resetTest();
  gEvents = createEventNode(year, day, TILLAGE, "0.2");
  gEvents->nextEvent = createEventNode(currentYear, currentDay, TILLAGE, "0.3");
  setupEvents();
  // The first climate record precedes both events.
  processEventsForCarbon(gEvent);
  climate->year = currentYear;
  climate->day = currentDay;

  should_exit = 1;
  expected_code = EXIT_CODE_INPUT_FILE_ERROR;
  exit_result = 1;
  int jumped = setjmp(jump_env);
  if (!jumped) {
    processEventsForCarbon(gEvent);
  }
  test_assert(jumped == 1);
  test_assert(eventTrackers.d_till_mod == 0.0);
  test_assert(gEvent == gEvents);
  if (!exit_result) {
    logTest("Missed event %d/%d at climate %d/%d was not rejected\n", year, day,
            currentYear, currentDay);
  }
  return !exit_result;
}

static void processDate(int year, int day) {
  climate->year = year;
  climate->day = day;
  fluxes = (Fluxes){0};
  processEventsForCarbon(gEvent);
  processEventsForNitrogen(gEvent);
  writeEventsOut();
}

static int testValidSchedule(int nitrogen) {
  resetTest();
  updateIntContext("nitrogenCycle", nitrogen, CTX_TEST);
  updateIntContext("litterPool", 1, CTX_TEST);
  gEvents = createEventNode(2016, 200, TILLAGE, "0.2");
  EventNode *nextYear = createEventNode(2017, 10, TILLAGE, "0.3");
  gEvents->nextEvent = nextYear;
  nextYear->nextEvent = createEventNode(2017, 10, FERTILIZATION, "2 3 4");
  setupEvents();
  eventOutFile = tmpfile();
  if (!eventOutFile) {
    logTest("Unable to create temporary event output\n");
    return 1;
  }

  // Any unexpected exit in the successful schedule must fail the executable.
  really_exit = 1;
  int status = 0;
  processDate(2016, 100);
  status |= gEvent != gEvents || eventTrackers.d_till_mod != 0.0;
  processDate(2016, 200);
  status |= gEvent != nextYear;
  status |= !compareDoubles(eventTrackers.d_till_mod, 0.2);
  // A lower day-of-year in a future year must not be processed early.
  processDate(2016, 250);
  status |= gEvent != nextYear || fluxes.eventInputN != 0.0;
  processDate(2017, 10);
  status |= gEvent != NULL;
  status |= !compareDoubles(eventTrackers.d_till_mod, 0.5);
  status |= !compareDoubles(fluxes.eventLitterC, 3.0);
  status |= !compareDoubles(fluxes.eventInputN, nitrogen ? 6.0 : 0.0);
  // A second climate record that day must not apply any event again.
  processDate(2017, 10);
  status |= !compareDoubles(eventTrackers.d_till_mod, 0.5);
  status |= fluxes.eventLitterC != 0.0 || fluxes.eventInputN != 0.0;
  processDate(2017, 11);  // empty queue

  rewind(eventOutFile);
  int year, day;
  char line[1024], type[32];
  int rows = 0;
  while (fgets(line, sizeof(line), eventOutFile)) {
    if (sscanf(line, "%d %d %31s", &year, &day, type) != 3) {
      status = 1;
      continue;
    }
    status |= year != (rows == 0 ? 2016 : 2017);
    status |= day != (rows == 0 ? 200 : 10);
    status |= strcmp(type, rows == 2 ? "fert" : "till") != 0;
    rows++;
  }
  status |= rows != 3;
  closeEventOutFile();
  return status;
}

int main(void) {
  initContext();
  updateIntContext("events", 1, CTX_TEST);
  int status = testMissedEvent(2016, 200, 2017, 10);
  status |= testMissedEvent(2016, 200, 2017, 200);
  status |= testMissedEvent(2016, 200, 2017, 250);
  status |= testMissedEvent(2016, 200, 2016, 210);
  status |= testValidSchedule(0);
  status |= testValidSchedule(1);
  resetTest();
  logTest("%s testEventDateChecks\n", status ? "FAILED" : "PASSED");
  return status;
}

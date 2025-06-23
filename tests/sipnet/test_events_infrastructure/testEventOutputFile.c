
#include <stdio.h>
#include <stdlib.h>

#include "utils/tUtils.h"
#include "common/logging.h"
#include "sipnet/sipnet.c"
#include "sipnet/events.c"

void init(void) {
  // Values here don't matter, just want to make sure they are initialized to
  // something. Value checks are in other tests.
  envi.litter = 1;
  envi.plantLeafC = 2;
  envi.plantWoodC = 3;
  envi.fineRootC = 4;
  envi.coarseRootC = 5;
  envi.soilWater = 0;
  fluxes.immedEvap = 0;
  params.immedEvapFrac = 0.5;

  // set up dummy climate
  ClimateNode *climate1 = (ClimateNode *)malloc(sizeof(ClimateNode));
  ClimateNode *climate2 = (ClimateNode *)malloc(sizeof(ClimateNode));
  ClimateNode *climate3 = (ClimateNode *)malloc(sizeof(ClimateNode));
  climate1->year = 2023;
  climate1->day = 65;
  climate2->year = 2023;
  climate2->day = 70;
  climate3->year = 2023;
  climate3->day = 200;
  ClimateNode *climate4 = (ClimateNode *)malloc(sizeof(ClimateNode));
  ClimateNode *climate5 = (ClimateNode *)malloc(sizeof(ClimateNode));
  ClimateNode *climate6 = (ClimateNode *)malloc(sizeof(ClimateNode));
  climate4->year = 2024;
  climate4->day = 65;
  climate5->year = 2024;
  climate5->day = 70;
  climate6->year = 2024;
  climate6->day = 200;

  climate5->nextClim = climate6;
  climate4->nextClim = climate5;
  climate3->nextClim = climate4;
  climate2->nextClim = climate3;
  climate1->nextClim = climate2;
  climate = climate1;
}

void runLoc(void) {
  ClimateNode *climStart = climate;
  setupEvents();
  while (climate != NULL) {
    processEvents();
    climate = climate->nextClim;
  }
  climate = climStart;
}

int runTest(const char *prefix, int header) {
  int status = 0;

  char input[30];
  char output[30];

  strcpy(input, prefix);
  strcat(input, ".in");
  strcpy(output, prefix);
  strcat(output, ".out");

  initEvents(input, header);
  runLoc();

  closeEventOutFile(eventOutFile);
  status = diffFiles(output, "events.out");

  return status;
}

int run(void) {

  int testStatus = 0;
  int status;
  initContext();
  updateIntContext("events", 1, CTX_TEST);

  status = runTest("events_output_no_header", 0);
  if (status) {
    logTest("runTest(no_header) failed\n");
  }
  testStatus |= status;

  status = runTest("events_output_header", 1);
  if (status) {
    logTest("runTest(header) failed\n");
  }
  testStatus |= status;

  return testStatus;
}

int main(void) {
  int status;

  // Set up params and env to make sure event processing runs
  init();

  logTest("Starting run()\n");
  status = run();
  if (status) {
    logTest("FAILED testEventOutputFile with status %d\n", status);
    exit(status);
  }

  logTest("PASSED testEventOutputFile\n");
}

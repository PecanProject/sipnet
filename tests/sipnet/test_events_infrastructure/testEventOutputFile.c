
#include <stdio.h>
#include <stdlib.h>

#include "modelStructures.h"  // NOLINT
#include "utils/tUtils.h"
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
  climate1->year = 2024;
  climate1->day = 65;
  climate2->year = 2024;
  climate2->day = 70;
  climate3->year = 2024;
  climate3->day = 200;

  climate2->nextClim = climate3;
  climate1->nextClim = climate2;
  climate = climate1;
}

void runLoc(int loc) {
  ClimateNode *climStart = climate;
  setupEvents(loc);
  while (climate != NULL) {
    processEvents();
    climate = climate->nextClim;
  }
  climate = climStart;
}

int checkOutputFile(const char *outputFile) {
  FILE *file2 = fopen(outputFile, "rb");
  FILE *file1 = fopen("events.out", "rb");

  if (file1 == NULL || file2 == NULL) {
    printf("Error opening files\n");
    return 1;
  }

  int char1, char2;
  int status = 0;

  while (1) {
    char1 = fgetc(file1);
    char2 = fgetc(file2);

    if (char1 == EOF && char2 == EOF) {
      break;
    } else if (char1 == EOF || char2 == EOF || char1 != char2) {
      char command[80];
      sprintf(command, "diff %s %s", outputFile, "events.out");
      system(command);
      status = 1;
      break;
    }
  }

  fclose(file1);
  fclose(file2);

  return status;
}

int runTest(const char *prefix, int header) {
  int status = 0;
  int numLocs = 1;

  char input[30];
  char output[30];

  strcpy(input, prefix);
  strcat(input, ".in");
  strcpy(output, prefix);
  strcat(output, ".out");

  initEvents(input, numLocs, header);
  runLoc(0);
  runLoc(1);

  closeEventOutFile(eventOutFile);
  status = checkOutputFile(output);

  return status;
}

int run(void) {

  int testStatus = 0;
  int status;
  status = runTest("events_output_no_header", 0);
  if (status) {
    printf("runTest(no_header) failed\n");
  }
  testStatus |= status;

  status = runTest("events_output_header", 1);
  if (status) {
    printf("runTest(header) failed\n");
  }
  testStatus |= status;

  return testStatus;
}

int main(void) {
  int status;

  // Set up params and env to make sure event processing runs
  init();

  printf("Starting run()\n");
  status = run();
  if (status) {
    printf("FAILED testEventOutputFile with status %d\n", status);
    exit(status);
  }

  printf("PASSED testEventOutputFile\n");
}

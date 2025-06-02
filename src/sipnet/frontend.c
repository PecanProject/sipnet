// Bill Sacks
// 7/8/02

// front end for sipnet - set up environment and call the appropriate run
// function

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>  // for command-line arguments

#include "common/exitCodes.h"
#include "common/namelistInput.h"
#include "common/spatialParams.h"
#include "common/util.h"

#include "events.h"
#include "sipnet.h"
#include "outputItems.h"
#include "modelStructures.h"

// important constants - default values:

#define FILE_MAXNAME 256
#define RUNTYPE_MAXNAME 16
#define INPUT_MAXNAME 64
#define INPUT_FILE "sipnet.in"
#define DO_MAIN_OUTPUT 1
#define DO_SINGLE_OUTPUTS 0
// Default is run at all locations
#define LOC (-1)

void checkRuntype(NamelistInputs *namelist, const char *inputFile) {
  NamelistInputItem *inputItem = locateNamelistInputItem(namelist, "RUNTYPE");
  if (inputItem == NULL) {
    // Well, something really broke
    printf("ERROR: did not find NameListInputItem RUNTYPE\n");
    exit(EXIT_CODE_INTERNAL_ERROR);
  }
  if (!(inputItem->wasRead)) {
    strcpy((char *)inputItem->ptr, "standard");
    return;
  }
  if (strcmpIgnoreCase((char *)inputItem->ptr, "standard") != 0) {
    // Make sure this is not an old config with a different RUNTYPE set
    printf("SIPNET only supports the standard runtype mode; other options are "
           "obsolete and were last supported in v1.3.0\n");
    printf("Please fix %s and re-run\n", inputFile);
    exit(EXIT_CODE_BAD_PARAMETER_VALUE);
  }
}

void usage(char *progName) {
  printf("Usage: %s [-h] [-i inputFile]\n", progName);
  printf("[-h] : Print this usage message and exit\n");
  printf("[-i inputFile]: Use given input file to configure the run\n");
  printf("\tDefault: %s\n", INPUT_FILE);
}

int main(int argc, char *argv[]) {
  char inputFile[INPUT_MAXNAME] = INPUT_FILE;
  NamelistInputs *namelistInputs;

  FILE *out;
  int option;  // reading in optional arguments

  SpatialParams *spatialParams;  // the parameters used in the model (possibly
                                 // spatially-varying)
  OutputItems *outputItems;  // structure to hold information for output to
                             // single-variable files (if doSingleOutputs is
                             // true)

  char runtype[RUNTYPE_MAXNAME];

  int doMainOutput = DO_MAIN_OUTPUT;  // do we do main outputting of all
                                      // variables?
  int doSingleOutputs = DO_SINGLE_OUTPUTS;  // do we do extra outputting of
                                            // single-variable files?
  int loc = LOC;  // location to run at (set through optional -l argument)
  int numLocs;  // read in initModel
  int *steps;  // number of time steps in each location

  int printHeader = HEADER;

  char fileName[FILE_MAXNAME];
  char outFile[FILE_MAXNAME + 24];
  char paramFile[FILE_MAXNAME + 24], climFile[FILE_MAXNAME + 24];

  // get command-line arguments:
  while ((option = getopt(argc, argv, "hi:")) != -1) {
    // we have another optional argument
    switch (option) {
      case 'h':
        usage(argv[0]);
        exit(1);
      case 'i':
        if (strlen(optarg) >= INPUT_MAXNAME) {
          printf("ERROR: input filename %s exceeds maximum length of %d\n",
                 optarg, INPUT_MAXNAME);
          printf("Either change the name or increase INPUT_MAXNAME in "
                 "frontend.c\n");
          exit(1);
        }
        strcpy(inputFile, optarg);
        break;
      default:
        usage(argv[0]);
        exit(1);
    }
  }

  // clang-format off
  // setup namelist input:
  namelistInputs = newNamelistInputs();
  addNamelistInputItem(namelistInputs, "RUNTYPE", STRING_TYPE, runtype, RUNTYPE_MAXNAME);
  addNamelistInputItem(namelistInputs, "FILENAME", STRING_TYPE, fileName, FILE_MAXNAME);
  addNamelistInputItem(namelistInputs, "LOCATION", INT_TYPE, &loc,0);
  addNamelistInputItem(namelistInputs, "DO_MAIN_OUTPUT", INT_TYPE, &doMainOutput, 0);
  addNamelistInputItem(namelistInputs, "DO_SINGLE_OUTPUTS", INT_TYPE, &doSingleOutputs, 0);
  addNamelistInputItem(namelistInputs, "PRINT_HEADER", INT_TYPE, &printHeader,0);
  // clang-format on

  // read from input file:
  readNamelistInputs(namelistInputs, inputFile);

  // Make sure FILENAME is set; everything else is optional (not necessary or
  // has a default)
  dieIfNotSet(namelistInputs, "FILENAME");

  // Handle RUNTYPE as an obsolete param; if it isn't set, consider it to be
  // set to "standard"; and make sure it is that if set
  checkRuntype(namelistInputs, inputFile);

  strcpy(paramFile, fileName);
  strcat(paramFile, ".param");
  strcpy(climFile, fileName);
  strcat(climFile, ".clim");
  numLocs = initModel(&spatialParams, &steps, paramFile, climFile);

#if EVENT_HANDLER
  initEvents(EVENT_IN_FILE, numLocs, printHeader);
#endif

  if (doSingleOutputs) {
    outputItems = newOutputItems(fileName, ' ');
    setupOutputItems(outputItems);
  } else {
    outputItems = NULL;
  }

  // Do the run!
  if (doMainOutput) {
    strcpy(outFile, fileName);
    strcat(outFile, ".out");
    out = openFile(outFile, "w");
  } else {
    out = NULL;
  }

  runModelOutput(out, outputItems, printHeader, spatialParams, loc);

  if (doMainOutput) {
    fclose(out);
  }

  cleanupModel(numLocs);
  deleteSpatialParams(spatialParams);
  if (outputItems != NULL) {
    deleteOutputItems(outputItems);
  }
  free(steps);

  return 0;
}

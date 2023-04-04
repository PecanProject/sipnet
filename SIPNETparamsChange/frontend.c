// Bill Sacks
// 7/8/02

// front end for sipnet - set up environment and call the appropriate run function

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h> // for command-line arguments
#include "sipnet.h"
#include "util.h"

#define NUM_STEPS 4507 // only used if we want to output means/standard deviations over a set of parameter sets

// important constants - default values:

#define LOC -1 // default is run at all locations (but if doing a sens. test, will default to running at loc. 0)
#define HEADER 0 // // Make the default no printing of header files

void usage(char *progName) {
  printf("Usage: %s fileName, where:\n", progName);
  printf("fileName.param is the file of parameter values and initial conditions\n");
  printf("fileName.param-spatial is the file of spatially-varying parameters (first line must contain a single integer: # of locations)\n");
  printf("fileName.clim is the file of climate data for each time step.\n\n");
  printf("OR: %s fileName paramValuesFile numToSkip outFileName, where:\n", progName);
  printf("paramValuesFile is a file of parameter values to use in multiple runs,\n");
  printf("in place of fileName.param.\n");
  printf("The first line of paramValues contains the indices of the parameters whose values are given\n");
  printf("and each subsequent line contains one set of parameter values.\n");
  printf("numToSkip is the number of elements on each line to ignore (e.g. leading likelihood values\n");
  printf("output is put in outFileName#.out\n");
  printf("Note that it is only possible to run at a single location using this option.\n\n");
  printf("OR: %s fileName changeIndex lowVal highVal numRuns, where:\n", progName);
  printf("changeIndex is index of parameter to change in sensitivity test\n");
  printf("parameter is varied from lowVal to highVal over a total of numRuns\n");
  printf("Output from sensitivity test put in fileName.sens\n");
  printf("\nThe following are optional arguments, with their default values:\n");
  printf("[ -l loc ] : location to run at (-1 means run at all locations) {%d}\n", LOC);
  printf("[ -h 1   ] : Print header on output file\n");
  printf("\tNote: If doing a sensitivity test, loc defaults to 0\n");
  printf("\nNOTE: Must specify optional arguments BEFORE any required arguments\n");
}


int main(int argc, char *argv[]) {
  FILE *out, *pChange; 
  char line[1024];
  char *errc;
  char option; // reading in optional arguments

  SpatialParams *spatialParams; // the parameters used in the model (possibly spatially-varying)
  int loc = LOC; // location to run at (set through optional -l argument)
  int numLocs; // read in initModel
  int *steps; /* number of time steps in each location;
		 as of 8/30/04, unused: but we save it so we can free it */

  int printHeader=HEADER;

  // parameters for sens. test:
  int changeIndex, numRuns;
  double lowVal, highVal;

  int numToSkip; // number of #'s to skip at start of each line of pChange
  int numChangeableParams, i; // in pChange
  int *indices; // indices of changeable params
  double value; // one value from file
  char outFile[256];
  char paramFile[256], climFile[256];
  int runNum;

  // variables used for outputting means/standard deviations over a set of parameter sets:
  int j, k, type;
  double **model;
  double **means;
  double **standarddevs;
  int dataTypeIndices[MAX_DATA_TYPES];

	

  // get command-line arguments:
  while ((option = getopt(argc, argv, "l:h:")) != -1) {
    // we have another optional argument
    switch(option) {
    case 'l':
      loc = strtol(optarg, &errc, 0);
      break;
    case 'h':
      printHeader = strtol(optarg, &errc, 0);
      break;
    default:
      usage(argv[0]);
      exit(1);
    }

    if (strlen(errc) > 0) { // invalid character(s) in argument
      printf("Invalid value: %s\n", optarg);
      exit(1);
    }
  }
  // now optind points to first non-optional argument (i.e. fileName)

  if (argc != (optind + 1) && argc != (optind + 4) && argc != (optind + 5)) { // expecting 1, 4, or 5 more arguments
    usage(argv[0]);
    exit(1);
  }

  strcpy(paramFile, argv[optind]);
  strcat(paramFile, ".param");
  strcpy(climFile, argv[optind]);
  strcat(climFile, ".clim");
  numLocs = initModel(&spatialParams, &steps, paramFile, climFile);

  if (argc == optind + 1) { // do a single run
    strcpy(outFile, argv[optind]);
    strcat(outFile, ".out");

    out = openFile(outFile, "w");

    runModelOutput(out, printHeader, spatialParams, loc); 

    fclose(out);
  }

  else if (argc == optind + 4) { // multiple runs from file
    if (loc == -1) {
      printf("loc was set to -1: can only run multiple runs from file at one location: running at location 0\n");
      loc = 0;
    }

    pChange = openFile(argv[optind + 1], "r");

    // first find number of changeable parameters:
    fgets(line, sizeof(line), pChange);
    strtok(line, " \t"); // read and ignore first token -- split on space and tab
    numChangeableParams = 1; // assume at least one changeableParam
    while (strtok(NULL, " \t") != NULL) // now count # of remaining tokens (ie indices)
      numChangeableParams++;

    // now allocate space for arrays and find the param indices:
    indices = (int *)malloc(numChangeableParams * sizeof(int));
    rewind(pChange);
    fgets(line, sizeof(line), pChange);
    indices[0] = strtol(strtok(line, " \t"), &errc, 0);
    for (i = 1; i < numChangeableParams; i++)
      indices[i] = strtol(strtok(NULL, " \t"), &errc, 0);

    numToSkip = strtol(argv[optind + 2], &errc, 0);

#if 0

    // do each run, output to fileName#.out
    runNum = 1;
    while((fgets(line, sizeof(line), pChange) != NULL) && (strcmp(line, "\n") != 0) && (strcmp(line, "\r\n") != 0)) { 
      // get next set of parameter values and do next run
      sprintf(outFile, "%s%d.out", argv[optind + 3], runNum);
      out = openFile(outFile, "w");
      if (numToSkip > 0) {
	strtok(line, " \t"); // read and ignore first value
	for (i = 0; i < numToSkip - 1; i++) // read and ignore other values
	  strtok(NULL, " \t"); 
	// now get first real value:
	value = strtod(strtok(NULL, " \t"), &errc);
      }
      else // just get first real value
	value = strtod(strtok(line, " \t"), &errc);
      setSpatialParam(spatialParams, indices[0], loc, value); // set value of changeable parameter #0
      // now get remaining values:
      for (i = 1; i < numChangeableParams; i++) {
	value = strtod(strtok(NULL, " \t"), &errc);
	setSpatialParam(spatialParams, indices[i], loc, value); // set value of changeable parameter #i
      }

      // do this model run:
      runModelOutput(out, printHeader, spatialParams, loc); 
      fclose(out);
      runNum++;
    }

#else 

    model = make2DArray(NUM_STEPS, MAX_DATA_TYPES);
    means = make2DArray(NUM_STEPS, MAX_DATA_TYPES);
    standarddevs = make2DArray(NUM_STEPS, MAX_DATA_TYPES);

    // fill dataTypeIndices: use all data types
    for (i = 0; i < MAX_DATA_TYPES; i++)
      dataTypeIndices[i] = i;

    // do each run, only output means and standard devs. of NEE to fileName.out:
    // first pass: compute means; second pass: compute standard deviations
    
    for (i = 0; i < NUM_STEPS; i++) {
      for (type = 0; type < MAX_DATA_TYPES; type++) {
	means[i][type] = 0.0;
	standarddevs[i][type] = 0.0;
      }
    }

    for (k = 0; k < 2; k++) {
      runNum = 1;
      while((fgets(line, sizeof(line), pChange) != NULL) && (strcmp(line, "\n") != 0) && (strcmp(line, "\r\n") != 0)) { 
	// get next set of parameter values and do next run
	if (numToSkip > 0) {
	  strtok(line, " \t"); // read and ignore first value
	  for (i = 0; i < numToSkip - 1; i++) // read and ignore other values
	    strtok(NULL, " \t"); 
	  // now get first real value:
	  value = strtod(strtok(NULL, " \t"), &errc);
	}
	else // just get first real value
	  value = strtod(strtok(line, " \t"), &errc);
	setSpatialParam(spatialParams, indices[0], loc, value); // set value of changeable parameter #0
	// now get remaining values:
	for (i = 1; i < numChangeableParams; i++) {
	  value = strtod(strtok(NULL, " \t"), &errc);
	  setSpatialParam(spatialParams, indices[i], loc, value); // set value of changeable parameter #i
	}

	// do this model run:
	runModelNoOut(model, MAX_DATA_TYPES, dataTypeIndices, spatialParams, loc);
	
	if (k == 0) { // first pass
	  // update means:
	  for (j = 0; j < NUM_STEPS; j++)
	    for (type = 0; type < MAX_DATA_TYPES; type++)
	      means[j][type] += model[j][type];
	}
	else { // second pass
	  // update standard deviations:
	  for (j = 0; j < NUM_STEPS; j++)
	    for (type = 0; type < MAX_DATA_TYPES; type++)
	      standarddevs[j][type] += pow((model[j][type] - means[j][type]), 2);
	}

	runNum++;
      }

      runNum--; // correct for one extra addition
      if (k == 0) { // first pass
	// make means means rather than sums:
	for (j = 0; j < NUM_STEPS; j++)
	  for (type = 0; type < MAX_DATA_TYPES; type++)
	    means[j][type] = means[j][type]/(double)runNum;
	
	// reset for second pass:
	rewind(pChange);
	fgets(line, sizeof(line), pChange); // read and ignore first line
      }

      else { // second pass
	// make standard devs standard devs rather than sum of squares:
	for (j = 0; j < NUM_STEPS; j++)
	  for (type = 0; type < MAX_DATA_TYPES; type++)
	    standarddevs[j][type] = sqrt(standarddevs[j][type]/(double)(runNum - 1));

	// write means and standard dev's to file:
	sprintf(outFile, "%s.out", argv[optind + 3]);
	out = openFile(outFile, "w");
	
	for (j = 0; j < NUM_STEPS; j++) {
	  for (type = 0; type < MAX_DATA_TYPES; type++)
	    fprintf(out, "%f %f\t", means[j][type], standarddevs[j][type]);
	  fprintf(out, "\n");
	}
	
	fclose(out);
      }      
    }

    free2DArray((void **)model);
    free2DArray((void **)means);
    free2DArray((void **)standarddevs);

#endif

    fclose(pChange);
    free(indices);
  }

  else if (argc == optind + 5) { // do a sensitivity test
    strcpy(outFile, argv[optind]);
    strcat(outFile, ".sens");
    out = openFile(outFile, "w");

    changeIndex = strtol(argv[optind + 1], &errc, 0);
    lowVal = strtod(argv[optind + 2], &errc);
    highVal = strtod(argv[optind + 3], &errc);
    numRuns = strtol(argv[optind + 4], &errc, 0);

    if (loc == -1) {
      printf("loc was set to -1: can only run sens. test at one location: running at location 0\n");
      loc = 0;
    }
    sensTest(out, changeIndex, lowVal, highVal, numRuns, spatialParams, loc);
    fclose(out);
  }
    
  cleanupModel(numLocs);
  deleteSpatialParams(spatialParams);
  free(steps);
  
  return 0;
}


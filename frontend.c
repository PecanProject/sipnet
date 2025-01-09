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
#include "spatialParams.h"
#include "namelistInput.h"
#include "outputItems.h"
#include "modelStructures.h"

// important constants - default values:

#define FILE_MAXNAME 256
#define RUNTYPE_MAXNAME 16
#define INPUT_MAXNAME 64
#define INPUT_FILE "sipnet.in"
#define DO_MAIN_OUTPUT 1
#define DO_SINGLE_OUTPUTS 0
#define LOC -1 // default is run at all locations (but if doing a sens. test or monte carlo run, will default to running at loc. 0)
#define HEADER 0 // // Make the default no printing of header files


void usage(char *progName)  {
  printf("Usage: %s [-h] [-i inputFile]\n", progName);
  printf("[-h] : Print this usage message and exit\n");
  printf("[-i inputFile]: Use given input file to configure the run\n");
  printf("\tDefault: %s\n", INPUT_FILE);
}


int main(int argc, char *argv[]) {
  char inputFile[INPUT_MAXNAME] = INPUT_FILE;
  NamelistInputs *namelistInputs;

  FILE *out, *pChange; 
  char line[8192];  // allow this to be long, since it might have to store lots of parameter names
  char *errc;
  char option; // reading in optional arguments

  SpatialParams *spatialParams; // the parameters used in the model (possibly spatially-varying)
  OutputItems *outputItems;  // structure to hold information for output to single-variable files (if doSingleOutputs is true)
  
  char runtype[RUNTYPE_MAXNAME];

  int doMainOutput = DO_MAIN_OUTPUT;  // do we do main outputting of all variables?
  int doSingleOutputs = DO_SINGLE_OUTPUTS;  // do we do extra outputting of single-variable files?
  int loc = LOC; // location to run at (set through optional -l argument)
  int numLocs; // read in initModel
  int *steps; // number of time steps in each location

  int printHeader=HEADER;

  // parameters for sens. test:
  char changeParam[PARAM_MAXNAME];
  int changeIndex;  // determined dynamically from changeParam
  int numRuns;
  double lowVal, highVal;

  int numToSkip = 0; // number of #'s to skip at start of each line of pChange
  int statsOnly = 0; // do we only output means & standard dev's for a montecarlo run?
  int numChangeableParams, i; // in pChange
  int *indices; // indices of changeable params
  char *paramName;  // name of one of the parameters that varies for a montecarlo run
  double value; // one value from file
  char fileName[FILE_MAXNAME];
  char outFile[FILE_MAXNAME+24];
  char paramFile[FILE_MAXNAME+24], climFile[FILE_MAXNAME+24];
  char mcParamFile[FILE_MAXNAME], mcOutFileBase[FILE_MAXNAME];  // used for runtype=montecarlo
  int runNum;

  // variables used for outputting means/standard deviations over a set of parameter sets:
  int j, k, type;
  double **model;
  double **means;
  double **standarddevs;
  int dataTypeIndices[MAX_DATA_TYPES];

#if EVENT_HANDLER
	// Extra filename if we are handing events
	char eventFile[FILE_MAXNAME+24];
#endif

  // get command-line arguments:
  while ((option = getopt(argc, argv, "hi:")) != -1) {
    // we have another optional argument
    switch(option) {
    case 'h':
      usage(argv[0]);
      exit(1);
      break;
    case 'i':
      if (strlen(optarg) >= INPUT_MAXNAME)  {
	printf("ERROR: input filename %s exceeds maximum length of %d\n", optarg, INPUT_MAXNAME);
	printf("Either change the name or increase INPUT_MAXNAME in frontend.c\n");
	exit(1);
      }
      strcpy(inputFile, optarg);
      break;
    default:
      usage(argv[0]);
      exit(1);
    }
  }

  // setup namelist input:
  namelistInputs = newNamelistInputs();
  addNamelistInputItem(namelistInputs, "RUNTYPE", STRING_TYPE, runtype, RUNTYPE_MAXNAME);
  addNamelistInputItem(namelistInputs, "FILENAME", STRING_TYPE, fileName, FILE_MAXNAME);
  addNamelistInputItem(namelistInputs, "LOCATION", INT_TYPE, &loc, 0);
  addNamelistInputItem(namelistInputs, "DO_MAIN_OUTPUT", INT_TYPE, &doMainOutput, 0);
  addNamelistInputItem(namelistInputs, "DO_SINGLE_OUTPUTS", INT_TYPE, &doSingleOutputs, 0);
  addNamelistInputItem(namelistInputs, "PRINT_HEADER", INT_TYPE, &printHeader, 0);
  addNamelistInputItem(namelistInputs, "CHANGE_PARAM", STRING_TYPE, changeParam, PARAM_MAXNAME);
  addNamelistInputItem(namelistInputs, "LOW_VAL", DOUBLE_TYPE, &lowVal, 0);
  addNamelistInputItem(namelistInputs, "HIGH_VAL", DOUBLE_TYPE, &highVal, 0);
  addNamelistInputItem(namelistInputs, "NUM_RUNS", INT_TYPE, &numRuns, 0);
  addNamelistInputItem(namelistInputs, "MC_PARAM_FILE", STRING_TYPE, mcParamFile, FILE_MAXNAME);
  addNamelistInputItem(namelistInputs, "MC_OUTPUT", STRING_TYPE, mcOutFileBase, FILE_MAXNAME);
  addNamelistInputItem(namelistInputs, "NUM_TO_SKIP", INT_TYPE, &numToSkip, 0);
  addNamelistInputItem(namelistInputs, "STATS_ONLY", INT_TYPE, &statsOnly, 0);

  // read from input file:
  readNamelistInputs(namelistInputs, inputFile);

  /* and make sure we read everything we needed to:
     (note that a few variables had default values, so it's okay if they weren't present in the input file) */
  dieIfNotSet(namelistInputs, "RUNTYPE");
  dieIfNotSet(namelistInputs, "FILENAME");
  if (strcmpIgnoreCase(runtype, "senstest") == 0)  {
    dieIfNotRead(namelistInputs, "CHANGE_PARAM");
    dieIfNotRead(namelistInputs, "LOW_VAL");
    dieIfNotRead(namelistInputs, "HIGH_VAL");
    dieIfNotRead(namelistInputs, "NUM_RUNS");
  }
  else if (strcmpIgnoreCase(runtype, "montecarlo") == 0)  {
    dieIfNotSet(namelistInputs, "MC_PARAM_FILE");
    dieIfNotSet(namelistInputs, "MC_OUTPUT");
  }

  // set values for ignored items:
  if ((strcmpIgnoreCase(runtype, "montecarlo") == 0) && (statsOnly == 1))  {
    doMainOutput = 1;  // it's silly for this to be false: means & standard dev's wouldn't be output!
    doSingleOutputs = 0;  // single outputs not implemented for this type of run
  }

  strcpy(paramFile, fileName);
  strcat(paramFile, ".param");
  strcpy(climFile, fileName);
  strcat(climFile, ".clim");
  numLocs = initModel(&spatialParams, &steps, paramFile, climFile);

#if EVENT_HANDLER
	strcpy(eventFile, fileName);
	strcat(eventFile, ".event");
	initEvents(eventFile, numLocs);
#endif

  if (doSingleOutputs)  {
    outputItems = newOutputItems(fileName, ' ');
    setupOutputItems(outputItems);
  }
  else  {
    outputItems = NULL;
  }

  if (strcmpIgnoreCase(runtype, "standard") == 0)  {  // do a single run
    if (doMainOutput)  {
      strcpy(outFile, fileName);
      strcat(outFile, ".out");
      out = openFile(outFile, "w");
    }
    else  {
      out = NULL;
    }

    runModelOutput(out, outputItems, printHeader, spatialParams, loc); 

    if (doMainOutput)
      fclose(out);
  }

  else if (strcmpIgnoreCase(runtype, "montecarlo") == 0)  {  // multiple runs from file
    if (loc == -1) {
      printf("loc was set to -1: can only run multiple runs from file at one location: running at location 0\n");
      loc = 0;
    }

    pChange = openFile(mcParamFile, "r");

    // first find number of changeable parameters:
    fgets(line, sizeof(line), pChange);
    strtok(line, " \t\n\r"); // read and ignore first token -- split on space, tab, newline & carriage return
    numChangeableParams = 1; // assume at least one changeableParam
    while (strtok(NULL, " \t\n\r") != NULL) // now count # of remaining tokens (i.e. # of parameter names)
      numChangeableParams++;

    // now allocate space for array and find the param indices:
    indices = (int *)malloc(numChangeableParams * sizeof(int));
    rewind(pChange);
    fgets(line, sizeof(line), pChange);
    paramName = strtok(line, " \t\n\r");  // get the first item
    for (i = 0; i < numChangeableParams; i++)  {
      indices[i] = locateParam(spatialParams, paramName);
      if (indices[i] == -1)  {
	printf("Invalid parameter '%s'\n", paramName);
	printf("Please fix first line of %s and re-run\n", mcParamFile);
	exit(1);
      }
      paramName = strtok(NULL, " \t\n\r");  /* get the next item (note: the last time this is called, we'll have paramName = NULL;
					   that's okay, because we just ignore it */
    }

    if (!statsOnly)  {
      // do each run, output to mcOutFileBase#.out (if doMainOutput is true),
      //  and/or mcOutFileBase.<dataTypeName> (if doSingleOutputs is true)

      runNum = 1;
      out = NULL;
      while((fgets(line, sizeof(line), pChange) != NULL) && (strcmp(line, "\n") != 0) && (strcmp(line, "\r\n") != 0)) { 
	// get next set of parameter values and do next run

	if (doMainOutput)  {
	  sprintf(outFile, "%s%d.out", mcOutFileBase, runNum);
	  out = openFile(outFile, "w");
	}
	// else out will stay NULL

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
	runModelOutput(out, outputItems, printHeader, spatialParams, loc); 
	if (doMainOutput)
	  fclose(out);
	runNum++;
      }

    }  // if (!statsOnly)

    else  {  // statsOnly

      model = make2DArray(steps[0], MAX_DATA_TYPES);
      means = make2DArray(steps[0], MAX_DATA_TYPES);
      standarddevs = make2DArray(steps[0], MAX_DATA_TYPES);

      // fill dataTypeIndices: use all data types
      for (i = 0; i < MAX_DATA_TYPES; i++)
	dataTypeIndices[i] = i;

      // do each run, only output means and standard devs. of NEE to mcOutFileBase.out:
      // first pass: compute means; second pass: compute standard deviations
    
      for (i = 0; i < steps[0]; i++) {
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
	    for (j = 0; j < steps[0]; j++)
	      for (type = 0; type < MAX_DATA_TYPES; type++)
		means[j][type] += model[j][type];
	  }
	  else { // second pass
	    // update standard deviations:
	    for (j = 0; j < steps[0]; j++)
	      for (type = 0; type < MAX_DATA_TYPES; type++)
		standarddevs[j][type] += pow((model[j][type] - means[j][type]), 2);
	  }

	  runNum++;
	}

	runNum--; // correct for one extra addition
	if (k == 0) { // first pass
	  // make means means rather than sums:
	  for (j = 0; j < steps[0]; j++)
	    for (type = 0; type < MAX_DATA_TYPES; type++)
	      means[j][type] = means[j][type]/(double)runNum;
	
	  // reset for second pass:
	  rewind(pChange);
	  fgets(line, sizeof(line), pChange); // read and ignore first line
	}

	else { // second pass
	  // make standard devs standard devs rather than sum of squares:
	  for (j = 0; j < steps[0]; j++)
	    for (type = 0; type < MAX_DATA_TYPES; type++)
	      standarddevs[j][type] = sqrt(standarddevs[j][type]/(double)(runNum - 1));

	  if (doMainOutput)  {  /* Note: it would be silly for doMainOutput to be false,
				   because if it were, the means and standard deviations
				   would not be output! */
	    // write means and standard dev's to file:
	    sprintf(outFile, "%s.out", mcOutFileBase);
	    out = openFile(outFile, "w");
	    
	    for (j = 0; j < steps[0]; j++) {
	      for (type = 0; type < MAX_DATA_TYPES; type++)
		fprintf(out, "%f %f\t", means[j][type], standarddevs[j][type]);
	      fprintf(out, "\n");
	    }
	    
	    fclose(out);
	  }
	}      
      }

      free2DArray((void **)model);
      free2DArray((void **)means);
      free2DArray((void **)standarddevs);

    }  // else (statsOnly)

    fclose(pChange);
    free(indices);
  }

  else if (strcmpIgnoreCase(runtype, "senstest") == 0)  {  // do a sensitivity test
    if (doMainOutput)  {
      strcpy(outFile, fileName);
      strcat(outFile, ".sens");
      out = openFile(outFile, "w");
    }
    else
      out = NULL;

    if (loc == -1) {
      printf("loc was set to -1: can only run sens. test at one location: running at location 0\n");
      loc = 0;
    }

    changeIndex = locateParam(spatialParams, changeParam);
    if (changeIndex == -1)  {
      printf("Invalid parameter '%s'\n", changeParam);
      printf("Please fix CHANGE_PARAM in sipnet.in and re-run\n");
      exit(1);
    }

    sensTest(out, outputItems, changeIndex, lowVal, highVal, numRuns, spatialParams, loc);
    if (doMainOutput)
      fclose(out);
  }

  else  {
    printf("ERROR in main: Unrecognized runtype: %s\n", runtype);
    printf("Please fix %s and re-run\n", inputFile);
  }
  
  cleanupModel(numLocs);
  deleteSpatialParams(spatialParams);
  if (outputItems != NULL)
    deleteOutputItems(outputItems);
  free(steps);
  
  return 0;
}


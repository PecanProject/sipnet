// Bill Sacks
// 7/15/02

// main function to run ml-metro parameter estimation

#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> // for command-line arguments
#include "sipnet.h"
#include "ml-metro.h"
#include "paramchange.h"
#include "util.h"
#include "spatialParams.h"

// important constants - default values:

#define ADD_PERCENT 0.5
#define COMPARE_INDICES_FILE "" // file giving start and end indices for post-comparison at each location; no file means use all points
#define FILE_FOR_AGG "" // no file
#define UNAGGED_WEIGHT 0.0 // if aggregation is done, give no weight to unaggregated points in optimization
#define NUM_CHAINS 10 // number of chains to run to convergence - pick best of these to use as start pt. for optimization
#define ITER 375000 // number of metropolis iterations once we've converged and finished numSpinUps
#define SCALE_FACTOR 1.0 // multiply LL difference by this
#define LOC -1 // default is run at all locations
#define NUM_RUNS 1
#define NUM_AT_ONCE 10000 // interval for checking convergence
#define PARAM_FILE "" // file to use in place of fileName.param - empty string means use fileName.param
#define RANDOM_START 0 // default is start with guess parameter values, NOT random parameter values
#define NUM_SPIN_UPS 125000 // once we've converged, additional number of iterations before we start recording
#define OPT_INDICES_FILE "" // file giving start and end indices for optimization at each location; no file means use all points
#define VALID_FRAC 0.5 /* fraction of data points which must be valid
		     to use data from a given time step */
#define PARAM_WEIGHT 0.0


void usage(char *progName) {
  char **dataTypeNames = getDataTypeNames(); // data types that can be used in optimization
  int i;

  printf("Usage: %s fileName outputName typeNum1 [typeNum2 ...], where:\n", progName);
  printf("fileName.param is the file of parameter values and info (unless -p option is specified)\n");
  printf("fileName.param-spatial is the file of spatially-varying parameters (first line must contain a single int: # of locations)\n");
  printf("\t(if -p option is specified, then use {paramFile}-spatial for spatially-varying parameters)\n");
  printf("fileName.clim is the file of climate data for each time step\n");
  printf("fileName.dat is the file of measured data (one column per data type)\n");
  //  printf("fileName.sigma is the file of sigmas for the data\n");
  printf("fileName.spd [Steps Per Day] contains one line per location; each line begins with year and julian day of 1st point,\n");
  printf("\tfollowed by the number of steps in each day, terminated by -1\n");
  printf("fileName.valid contains fraction of valid data points for each time step (one col. per data type)\n");
  printf("outputName will store user-readable info about the run\n");
  printf("outputName.param and outputName.param-spatial will store best parameter set found\n");
  printf("outputName.hist<n> will store history of run - i.e. every accepted parameter set - for each location n\n\n");
  printf("typeNum1 (and, optionally, typeNum2 ...) specify the data type(s) to use in optimization, as follows:\n");
  for (i = 0; i < MAX_DATA_TYPES; i++)
    printf("%d: %s\n", i, dataTypeNames[i]); 
 
  printf("\nThe following are optional arguments, with their default values:\n");
  printf("[ -a addPercent ] : initial pDelta, as percent of parameter range {%f}\n", ADD_PERCENT);
  printf("[ -c compareIndicesFile ] : file giving, for each location, start and end indices for model-data comparisons\n");
  printf("\t(one line per location, with each line containing two integers: start & end; if no file specified, use all points)\n");
  printf("\t(1-indexing; end = -1 means go to end) {%s}\n", COMPARE_INDICES_FILE);
  printf("[ -f fileforAgg ] : specify a file containing # of time steps per model/data aggregation\n");
  printf("\twhere each line contains aggregation info for one location, and each line is terminated with a -1\n");
  printf("\tEach value (i) on a given line is an integer giving the number of time steps in aggregation i in that time step\n");
  printf("\t(e.g. to run model-data comparison on a yearly aggregation, each value would be number of steps in year i)\n");
  printf("\t(if no file specified, will perform no aggregation) {%s}\n", FILE_FOR_AGG);
  printf("[ -g unAggedWeight ] : if aggregation is done, relative weight of unaggregated points in optimization\n");
  printf("\t(should probably be 0 or 1; 0 means only use aggregated points in optimization) {%f}\n", UNAGGED_WEIGHT);
  printf("[ -h numCHains ] : start by running numCHains chains to convergence,\n");
  printf("\tthen choosing the best of these as a starting point for the optimization\n");
  printf("\t(doing multiple starts helps prevent getting stuck in local optima) {%d}\n", NUM_CHAINS);
  printf("[ -i iter ] : number of metropolis iterations once we've converged and finished numSpinUps {%d}\n", ITER);
  printf("[ -k scaleFactor] : amount to multiply log likelihood difference by (anything but 1 goes against theory,\n");
  printf("\tunless used in conjunction with unAggedWeight) {%f}\n", SCALE_FACTOR);
  printf("[ -l loc ] : location to run at (-1 means run at all locations) {%d}\n", LOC);
  printf("[ -n numRuns ] : number of separate runs to perform {%d}\n", NUM_RUNS);
  printf("[ -o numAtOnce ] : to determine convergence: run for NUM_AT_ONCE iterations,\n");
  printf("\tthen check accept %% - if not close to A_STAR, run for another NUM_AT_ONCE iterations {%d}\n", NUM_AT_ONCE);
  printf("[ -p paramFile ] : instead of fileName.param, use given file for parameter values and info {%s}\n", PARAM_FILE);
  printf("[ -r randomStart ] : if 0, start chains with param. values = guess; if 1, start randomly b/t min and max {%d}\n", RANDOM_START);
  printf("[ -s numSpinUps ] : once temperatures have converged, number of additional iterations (Spin-ups)\n");
  printf("\tto run before we start recording (to allow posteriors to stabilize) {%d}\n", NUM_SPIN_UPS);
  printf("[ -t opTIndicesFile ] : file giving, for each location, start and end indices for optimization\n");
  printf("\t(one line per location, with each line containing two integers: start & end; if no file specified, use all points)\n");
  printf("\t(1-indexing; end = -1 means go to end) {%s}\n", OPT_INDICES_FILE);
  printf("[ -v validFrac ] : fraction of data points which must be valid\n");
  printf("\tto use data from a given time step {%f}\n", VALID_FRAC);
  printf("[ -w paramWeight ] : relative weight of param. vs. data error {%f}\n", PARAM_WEIGHT);
  printf("\nNOTE: On Mac, must specify optional arguments BEFORE any required arguments\n");
}


// initialize data indices array to [0,1,...,MAX_DATA_TYPES-1]
// returns size of array (= MAX_DATA_TYPES)
int initDataTypeIndices(int dataTypeIndices[]) {
  int i;
  for (i = 0; i < MAX_DATA_TYPES; i++)
    dataTypeIndices[i] = i;

  return MAX_DATA_TYPES;
}


/* set array of data types to only optimize on a restricted set of data types
   note: this is an interactive function
   NOTE2: THIS FUNCTION IS CURRENTLY UNUSED (as of 10/21/03)

   returns number of data types
*/
int setDataTypeIndices(int dataTypeIndices[]) {
  int numDataTypes;
  int i, j, index;
  int usedArray[MAX_DATA_TYPES]; // has the user already chosen a given data type?
  char **dataTypeNames = getDataTypeNames();

  // initialize usedArray:
  for (i = 0; i < MAX_DATA_TYPES; i++)
    usedArray[i] = 0; // false

  printf("Data types available for optimization:\n");
  for (i = 0; i < MAX_DATA_TYPES; i++)
    printf("%d: %s\n", i, dataTypeNames[i]); 
    
  printf("\n");

  // prompt for number of types:
  do {
    printf("Enter number of data types to use in optimization: ");
    scanf("%d", &numDataTypes);

    if (numDataTypes < 1 || numDataTypes > MAX_DATA_TYPES)
      printf("Must enter a number between 1 and %d\n", MAX_DATA_TYPES);
  } while (numDataTypes < 1 || numDataTypes > MAX_DATA_TYPES);

  // prompt for each index:
  i = 0;
  while (i < numDataTypes) {
    printf("\nData type %d of %d: Please choose one of the following:\n", i+1, numDataTypes);
    for (j = 0; j < MAX_DATA_TYPES; j++) {
      if (!usedArray[j]) // this data type hasn't been selected yet
	printf("%d: %s\n", j, dataTypeNames[j]); 
    }
    printf("Your choice: ");
    scanf("%d", &index);
    
    if (index < 0 || index >= MAX_DATA_TYPES || usedArray[index]) // out of bounds or already selected
      printf("Invalid choice.\n");
    else {
      dataTypeIndices[i] = index;
      usedArray[index] = 1; // this index is used
      i++;
    }
  } // end while (i < numDataTypes)

  return numDataTypes;
}

/* PRE: dataTypeIndices is at least big enough to hold MAX_DATA_TYPES ints
   argv is vector of command-line arguments
   startPos is index in argv where data type indices start
   argc is total argument count (i.e. number of elements in argv[])
   all arguments in argv from startPos to end are integers between 0 and (MAX_DATA_TYPES - 1), with no repeats

   POST: fills dataTypeIndices with given integer arguments
   returns number of data types
*/   
int parseDataTypeIndices(int dataTypeIndices[], char *argv[], int startPos, int argc) {
  int numDataTypes;
  int i, pos;
  int thisType;
  char *errc = "";
  int usedArray[MAX_DATA_TYPES]; // has a given data type already been chosen?

  numDataTypes = (argc - startPos); // how many arguments are left? they should all be data type indices
  
  // error checking:
  if (numDataTypes <= 0) { // uh-oh: no more arguments left
    usage(argv[0]);
    exit(1);
  }
  else if (numDataTypes > MAX_DATA_TYPES) { // uh-oh: too many arguments left
    printf("Error: more than %d data types specified\n", MAX_DATA_TYPES);
    exit(1);
  }

  // initialize usedArray:
  for (i = 0; i < MAX_DATA_TYPES; i++)
    usedArray[i] = 0; // false

  // read each data type:
  i = 0;
  pos = startPos;
  while (i < numDataTypes) { 
    thisType = strtol(argv[pos], &errc, 0); // convert string argument to integer

    // error check:
    if (thisType < 0 || thisType >= MAX_DATA_TYPES) {
      printf("Error: data type %d out of range [0..%d]\n", thisType, MAX_DATA_TYPES - 1);
      exit(1);
    }
    else if (usedArray[thisType]) {
      printf("Error: data type %d specified twice\n", thisType);
      exit(1);
    }

    // okay, thisType is valid
    dataTypeIndices[i] = thisType;
    usedArray[thisType] = 1; // this index is used

    i++;
    pos++;
  } // end while (i < numDataTypes)

  return numDataTypes;
}

// print what data types we're using to file
void printDataTypeIndices(int dataTypeIndices[], int numDataTypes, FILE *filePtr)
{
  int i;
  char **dataTypeNames = getDataTypeNames();
  
  fprintf(filePtr, "Data types used in optimization:\n");
  for (i = 0; i < numDataTypes; i++)
    fprintf(filePtr, "\t%s\n", dataTypeNames[dataTypeIndices[i]]); // constants defined in sipnet.h
}
    
  

int main(int argc, char *argv[]) {
  SpatialParams *spatialParams; // the parameters used in the model (possibly spatially-varying)
  int numLocs; // number of spatial locations: read from spatial param. file
  int *steps; // number of time steps in each location
  char option;
  double addPercent = ADD_PERCENT;
  char compareIndicesFile[64] = COMPARE_INDICES_FILE; // optional file holding indices for model-data comparisons
  char optIndicesFile[64] = OPT_INDICES_FILE; // optional file holding indices for optimizations
  char fileForAgg[64] = FILE_FOR_AGG; // optional file for aggregating 
  char paramFile[64] = PARAM_FILE; // if, after getting optional arguments, paramFile is "", set paramFile = fileName.param
  long iter = ITER, numSpinUps = NUM_SPIN_UPS;
  int numChains = NUM_CHAINS, numRuns = NUM_RUNS, numAtOnce = NUM_AT_ONCE, loc = LOC;
  int randomStart = RANDOM_START;
  double unaggedWeight = UNAGGED_WEIGHT, scaleFactor = SCALE_FACTOR, paramWeight = PARAM_WEIGHT;
  double validFrac = VALID_FRAC;
  int dataTypeIndices[MAX_DATA_TYPES]; // MAX_DATA_TYPES defined in sipnet.c
  int numDataTypes; // how many data types are we actually optimizing on?
  char *errc = "";
  int runNum;
  FILE *userOut;
  char inFileName[128], outFileName[256];
  char climFile[128]; // name of climate file
  char optionsFile[128]; // name of options file
  char thisFile[256]; // changes for each run
  char paramOutFile[256], spatialParamOutFile[256]; // for outputting best parameters
  void *differenceFunc = difference; /* the difference function to use (depends on whether we're aggregating model & data) 
					(default is plain old vanilla "difference") */

  numDataTypes = initDataTypeIndices(dataTypeIndices); // initialize array to [0,1,...,MAX_DATA_TYPES-1] (default)

  // get command-line arguments:
  while ((option = getopt(argc, argv, "a:c:f:g:h:i:k:l:n:o:p:r:s:t:v:w:")) != -1) { 
    // we have another optional argument
    switch(option) {
    case 'a':
      addPercent = strtod(optarg, &errc);
      break;
    case 'c':
      strcpy(compareIndicesFile, optarg);
      break;
    case 'f':
      strcpy(fileForAgg, optarg);
      break;
    case 'g':
      unaggedWeight = strtod(optarg, &errc);
      break;
    case 'h':
      numChains = strtol(optarg, &errc, 0);
      break;
    case 'i':
      iter = strtol(optarg, &errc, 0);
      break;
    case 'k':
      scaleFactor = strtod(optarg, &errc);
      break;
    case 'l':
      loc = strtol(optarg, &errc, 0);
      break;
    case 'n':
      numRuns = strtol(optarg, &errc, 0);
      break;
    case 'o':
      numAtOnce = strtol(optarg, &errc, 0);
      break;
    case 'p':
      strcpy(paramFile, optarg);
      break;
    case 'r':
      randomStart = strtol(optarg, &errc, 0);
      break;
    case 's':
      numSpinUps = strtol(optarg, &errc, 0);
      break;
    case 't':
      strcpy(optIndicesFile, optarg);
      break;
    case 'v':
      validFrac = strtod(optarg, &errc);
      break;
    case 'w':
      paramWeight = strtod(optarg, &errc);
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

  if (argc < (optind + 3)) { // 0, 1 or 2 more arguments - expecting 3 or more
    usage(argv[0]);
    exit(1);
  }

  strcpy(inFileName, argv[optind]);
  if (strcmp(paramFile, "") == 0) { // no alternative parameter file specified
    // set paramFile = {inFileName}.param
    strcpy(paramFile, inFileName);
    strcat(paramFile, ".param");
  }
  strcpy(climFile, inFileName);
  strcat(climFile, ".clim");

  strcpy(optionsFile, inFileName);
  strcat(optionsFile, ".options");

  strcpy(outFileName, argv[optind+1]);
  numDataTypes = parseDataTypeIndices(dataTypeIndices, argv, optind+2, argc);

  numLocs = initModel(&spatialParams, &steps, paramFile, climFile,optionsFile);

  userOut = openFile(outFileName, "w");

  fprintf(userOut, "Base input file name: %s\n", inFileName);
  fprintf(userOut, "Parameter file: %s\n", paramFile);
  fprintf(userOut, "numLocs = %d\n\n", numLocs);
  printModelComponents(userOut); // write to file which model componenets are turned on

  fprintf(userOut, "Constants:\n");
  fprintf(userOut, "ADD_PERCENT = %f\n", addPercent);
  fprintf(userOut, "COMPARE_INDICES_FILE = %s\n", compareIndicesFile);
  fprintf(userOut, "FILE_FOR_AGG = %s\n", fileForAgg);
  fprintf(userOut, "UNAGGED_WEIGHT = %f\n", unaggedWeight);
  fprintf(userOut, "NUM_CHAINS = %d\n", numChains);
  fprintf(userOut, "ITER = %ld\n", iter);
  fprintf(userOut, "SCALE_FACTOR = %f\n", scaleFactor);
  fprintf(userOut, "LOC = %d\n", loc);
  fprintf(userOut, "NUM_RUNS = %d\n", numRuns);
  fprintf(userOut, "NUM_AT_ONCE = %d\n", numAtOnce);
  // would print paramFile here, but it's already printed above
  fprintf(userOut, "RANDOM_START = %d\n", randomStart);
  fprintf(userOut, "NUM_SPIN_UPS = %ld\n", numSpinUps);
  fprintf(userOut, "OPT_INDICES_FILE = %s\n", optIndicesFile);
  fprintf(userOut, "VALID_FRAC = %f\n", validFrac);
  fprintf(userOut, "PARAM_WEIGHT = %f\n", paramWeight);
  printDataTypeIndices(dataTypeIndices, numDataTypes, userOut);
  fprintf(userOut, "\n\n");

  readData(inFileName, dataTypeIndices, numDataTypes, MAX_DATA_TYPES, numLocs, steps, 
	   validFrac, optIndicesFile, compareIndicesFile, userOut);

  if (strcmp(fileForAgg, "") != 0) { // there is a file for model-data aggregation
    readFileForAgg(fileForAgg, numDataTypes, unaggedWeight);
    differenceFunc = aggedDifference;
    numDataTypes *= 2; /* A bifurcation of data types: each data type is split into two data types:
			  an aggregated and an unaggregated.
			  But this bifurcation only applies to the collection of statistics, not
			  to the actual running of the model!
			  We'll have to "remember" this in the aggedDifference function!
		       */
  }

  signal(SIGINT,exit);
  seedRand(0, userOut);

  for (runNum = 1; runNum <= numRuns; runNum++) {
    fprintf(userOut, "Run #%d of %d:\n\n", runNum, numRuns);

    if (numRuns == 1)
      strcpy(thisFile, outFileName); // just use base file name
    else
      sprintf(thisFile, "%s_%d_", outFileName, runNum); /* append runNum to base file name
							   add extra underscore at end to separate run # from location #
							*/
    
    metropolis(thisFile, spatialParams, loc, differenceFunc, runModelNoOut, 
	       addPercent, iter, numAtOnce, numChains, randomStart, numSpinUps, paramWeight, scaleFactor,
	       dataTypeIndices, numDataTypes, userOut);

    strcpy(paramOutFile, thisFile);
    strcat(paramOutFile, ".param");
    strcpy(spatialParamOutFile, paramOutFile);
    strcat(spatialParamOutFile, "-spatial");

    writeBestSpatialParams(spatialParams, paramOutFile, spatialParamOutFile);
    fflush(NULL); /* flush all output streams
		    (so if we kill program, we'll still have all previous output) */
  }

  cleanupModel(numLocs);
  cleanupParamchange();
  deleteSpatialParams(spatialParams);
  free(steps);
  fclose(userOut); 

  return 0;
}

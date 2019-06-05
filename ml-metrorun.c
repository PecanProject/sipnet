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
#include "namelistInput.h"

// important constants - default values:

#define FILE_MAXNAME 256
#define INPUT_MAXNAME 64
#define INPUT_FILE "estimate.in"

#define ADD_FRACTION 0.5
#define COMPARE_INDICES_EXT "" // extension of file giving start and end indices for post-comparison at each location; no file means use all points
#define AGGREGATION_EXT "" // extension of file containing information for model/data aggregation; no file means don't do any aggregation
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
#define OPT_INDICES_EXT "" // extension of file giving start and end indices for optimization at each location; no file means use all points
#define VALID_FRAC 0.5 /* fraction of data points which must be valid
		     to use data from a given time step */
#define PARAM_WEIGHT 0.0


void usage(char *progName) {
  printf("Usage: %s [-h] [-i inputFile]\n", progName);
  printf("[-h] : Print this usage message and exit\n");
  printf("[-i inputFile]: Use given input file to configure the run\n");
  printf("\tDefault: %s\n", INPUT_FILE);
}


// initialize data indices array to [0,1,...,MAX_DATA_TYPES-1]
// returns size of array (= MAX_DATA_TYPES)
int initDataTypeIndices(int dataTypeIndices[]) {
  int i;
  for (i = 0; i < MAX_DATA_TYPES; i++)
    dataTypeIndices[i] = i;

  return MAX_DATA_TYPES;
}


/* PRE: dataTypeSwitches is of size MAX_DATA_TYPES
   dataTypeSwitches[i] is 1 if we are including this data type in optimization, 0 if not
   dataTypeIndices must be big enough to hold MAX_DATA_TYPES ints

   POST: dataTypeIndices[0..numDataTypes-1] contains the indices of the 1-valued elements in dataTypeSwitches
   - i.e. all of the data types included in optimization
   (remaining elements of dataTypeIndices are undefined)
   returns numDataTypes: number of data types included in optimization
 */
int parseDataTypeSwitches(int dataTypeIndices[], int dataTypeSwitches[])  {
  int numDataTypes;
  int i;

  numDataTypes = 0;
  for (i = 0; i < MAX_DATA_TYPES; i++)  {
    if (dataTypeSwitches[i])  {
      dataTypeIndices[numDataTypes] = i;
      numDataTypes++;
    }
  }

  return numDataTypes;
}


// print what data types we're using to file
void printDataTypeIndices(int dataTypeIndices[], int numDataTypes, FILE *filePtr)
{
  int i;
  char **dataTypeNames = getDataTypeNames();
  
  fprintf(filePtr, "Data types used in optimization:\n");
  for (i = 0; i < numDataTypes; i++)
    fprintf(filePtr, "\t%s\n", dataTypeNames[dataTypeIndices[i]]); 
}
    
  

int main(int argc, char *argv[]) {
  char inputFile[INPUT_MAXNAME] = INPUT_FILE;
  NamelistInputs *namelistInputs;

  SpatialParams *spatialParams; // the parameters used in the model (possibly spatially-varying)
  int numLocs; // number of spatial locations: read from spatial param. file
  int *steps; // number of time steps in each location
  char option;
  double addFraction = ADD_FRACTION;
  char compareIndicesExt[FILE_MAXNAME] = COMPARE_INDICES_EXT; // extension of optional file holding indices for model-data comparisons
  char compareIndicesFile[2*FILE_MAXNAME]; // optional file holding indices for model-data comparisons
  char optIndicesExt[FILE_MAXNAME] = OPT_INDICES_EXT; // extension of optional file holding indices for optimizations
  char optIndicesFile[2*FILE_MAXNAME]; // optional file holding indices for optimizations
  char aggregationExt[FILE_MAXNAME] = AGGREGATION_EXT; // extension of optional file for aggregating 
  char aggregationFile[2*FILE_MAXNAME]; // optional file for aggregating 
  char paramFile[FILE_MAXNAME] = PARAM_FILE; // if, after getting optional arguments, paramFile is "", set paramFile = fileName.param
  long iter = ITER, numSpinUps = NUM_SPIN_UPS;
  int numChains = NUM_CHAINS, numRuns = NUM_RUNS, numAtOnce = NUM_AT_ONCE, loc = LOC;
  int randomStart = RANDOM_START;
  double unaggedWeight = UNAGGED_WEIGHT, scaleFactor = SCALE_FACTOR, paramWeight = PARAM_WEIGHT;
  double validFrac = VALID_FRAC;
  int dataTypeIndices[MAX_DATA_TYPES]; // MAX_DATA_TYPES defined in sipnet.c
  int dataTypeSwitches[MAX_DATA_TYPES];  // 0 or 1 for each data type; used to build dataTypeIndices array
  int numDataTypes; // how many data types are we actually optimizing on?
  int runNum;
  FILE *userOut;
  char inFileName[FILE_MAXNAME], outFileName[FILE_MAXNAME];
  char climFile[FILE_MAXNAME+24]; // name of climate file
  char thisFile[FILE_MAXNAME+24]; // changes for each run
  char paramOutFile[FILE_MAXNAME+48], spatialParamOutFile[FILE_MAXNAME+48]; // for outputting best parameters
  void *differenceFunc = difference; /* the difference function to use (depends on whether we're aggregating model & data) 
					(default is plain old vanilla "difference") */
  char **dataTypeNames;
  char optTypeName[NAMELIST_INPUT_MAXNAME];  // names such as OPT_NEE, read in from input file
  int i;

  numDataTypes = initDataTypeIndices(dataTypeIndices); // initialize array to [0,1,...,MAX_DATA_TYPES-1] (default)

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
	printf("Either change the name or increase INPUT_MAXNAME in ml-metrorun.c\n");
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
  addNamelistInputItem(namelistInputs, "FILENAME", STRING_TYPE, inFileName, FILE_MAXNAME);
  addNamelistInputItem(namelistInputs, "PARAM_FILE", STRING_TYPE, paramFile, FILE_MAXNAME);
  addNamelistInputItem(namelistInputs, "OUTPUT_NAME", STRING_TYPE, outFileName, FILE_MAXNAME);
  addNamelistInputItem(namelistInputs, "LOC", INT_TYPE, &loc, 0);
  addNamelistInputItem(namelistInputs, "NUM_RUNS", INT_TYPE, &numRuns, 0);
  addNamelistInputItem(namelistInputs, "RANDOM_START", INT_TYPE, &randomStart, 0);
  addNamelistInputItem(namelistInputs, "NUM_AT_ONCE", INT_TYPE, &numAtOnce, 0);
  addNamelistInputItem(namelistInputs, "NUM_CHAINS", INT_TYPE, &numChains, 0);
  addNamelistInputItem(namelistInputs, "NUM_SPINUPS", LONG_TYPE, &numSpinUps, 0);
  addNamelistInputItem(namelistInputs, "ITER", LONG_TYPE, &iter, 0);
  addNamelistInputItem(namelistInputs, "ADD_FRACTION", DOUBLE_TYPE, &addFraction, 0);
  addNamelistInputItem(namelistInputs, "VALID_FRAC", DOUBLE_TYPE, &validFrac, 0);
  addNamelistInputItem(namelistInputs, "SCALE_FACTOR", DOUBLE_TYPE, &scaleFactor, 0);
  addNamelistInputItem(namelistInputs, "PARAM_WEIGHT", DOUBLE_TYPE, &paramWeight, 0);
  addNamelistInputItem(namelistInputs, "OPT_INDICES_EXT", STRING_TYPE, optIndicesExt, FILE_MAXNAME);
  addNamelistInputItem(namelistInputs, "COMPARE_INDICES_EXT", STRING_TYPE, compareIndicesExt, FILE_MAXNAME);
  addNamelistInputItem(namelistInputs, "AGGREGATION_EXT", STRING_TYPE, aggregationExt, FILE_MAXNAME);
  addNamelistInputItem(namelistInputs, "UNAGGED_WEIGHT", DOUBLE_TYPE, &unaggedWeight, 0);

  // one entry for each data type that can be included in optimization:
  dataTypeNames = getDataTypeNames();
  for (i = 0; i < MAX_DATA_TYPES; i++)  {
    if (strlen(dataTypeNames[i]) + 4 >= NAMELIST_INPUT_MAXNAME)  {
      printf("ERROR: OPT_%s is too long of a name for namelist input\n", dataTypeNames[i]);
      printf("Either change the name of this data type, or increase NAMELIST_INPUT_MAXNAME in namelistInput.h\n");
      exit(1);
    }
    strcpy(optTypeName, "OPT_");
    strcat(optTypeName, dataTypeNames[i]);

    addNamelistInputItem(namelistInputs, optTypeName, INT_TYPE, &(dataTypeSwitches[i]), 0);
    dataTypeSwitches[i] = 0;  // initialize to 0 in case it's not read from input file
  }

  // read from input file:
  readNamelistInputs(namelistInputs, inputFile);

  /* and make sure we read everything we needed to:
     (note that many variables had default values, so it's okay if they weren't present in the input file) */
  dieIfNotSet(namelistInputs, "FILENAME");
  dieIfNotSet(namelistInputs, "OUTPUT_NAME");

  numDataTypes = parseDataTypeSwitches(dataTypeIndices, dataTypeSwitches);
  if (numDataTypes == 0)  {
    printf("ERROR: Must specify at least one data type to include in optimization\n");
    printf("Please modify %s and re-run\n", inputFile);
    exit(1);
  }

  // Build optIndicesFile, compareIndicesFile, aggregationFile (file names):
  if (strcmp(optIndicesExt, "") != 0)
    buildFileName(optIndicesFile, inFileName, optIndicesExt);
  else
    strcpy(optIndicesFile, "");

  if (strcmp(compareIndicesExt, "") != 0)
    buildFileName(compareIndicesFile, inFileName, compareIndicesExt);
  else
    strcpy(compareIndicesFile, "");

  if (strcmp(aggregationExt, "") != 0)
    buildFileName(aggregationFile, inFileName, aggregationExt);
  else
    strcpy(aggregationFile, "");


  if (strcmp(paramFile, "") == 0) { // no alternative parameter file specified
    // set paramFile = {inFileName}.param
    buildFileName(paramFile, inFileName, "param");
  }
  buildFileName(climFile, inFileName, "clim");

  numLocs = initModel(&spatialParams, &steps, paramFile, climFile);

  userOut = openFile(outFileName, "w");

  fprintf(userOut, "Base input file name: %s\n", inFileName);
  fprintf(userOut, "Parameter file: %s\n", paramFile);
  fprintf(userOut, "numLocs = %d\n\n", numLocs);
  printModelComponents(userOut); // write to file which model componenets are turned on

  fprintf(userOut, "Constants:\n");
  fprintf(userOut, "ADD_FRACTION = %f\n", addFraction);
  fprintf(userOut, "COMPARE_INDICES_FILE = %s\n", compareIndicesFile);
  fprintf(userOut, "AGGREGATION_FILE = %s\n", aggregationFile);
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

  if (strcmp(aggregationFile, "") != 0) { // there is a file for model-data aggregation
    readFileForAgg(aggregationFile, numDataTypes, unaggedWeight);
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
	       addFraction, iter, numAtOnce, numChains, randomStart, numSpinUps, paramWeight, scaleFactor,
	       dataTypeIndices, numDataTypes, userOut);

    buildFileName(paramOutFile, thisFile, "param");
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

// Bill Sacks
// 2/23/05

// tests the sensitivity of model-data misfit to variations in a single parameter
// this currently only runs at a single location, though it could be extended to run at all locations
// (returning total likelihood summed across all locations)

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "sipnet.h"
#include "paramchange.h"
#include "util.h"
#include "spatialParams.h"
#include "namelistInput.h"

// important constants - default values:

#define FILE_MAXNAME 256
#define INPUT_MAXNAME 64
#define INPUT_FILE "sensTest.in"

#define AGGREGATION_EXT "" // extension of file containing information for model/data aggregation; no file means don't do any aggregation
#define UNAGGED_WEIGHT 0.0 // if aggregation is done, give no weight to unaggregated points in optimization
#define LOC 0 // default is run at location 0
#define PARAM_FILE "" // file to use in place of fileName.param - empty string means use fileName.param
#define OPT_INDICES_EXT "" // extension of file giving start and end indices for optimization at each location; no file means use all points
#define VALID_FRAC 0.5 /* fraction of data points which must be valid
		     to use data from a given time step */


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


int main(int argc, char *argv[]) {
  char inputFile[INPUT_MAXNAME] = INPUT_FILE;
  NamelistInputs *namelistInputs;

  SpatialParams *spatialParams; // the parameters used in the model (possibly spatially-varying)
  int numLocs; /* number of spatial locations in the various files: read from spatial param. file
		  note, though, that we only perform the sensitivity test at a single location */
  int *steps; // number of time steps in each location
  char option;
  char optIndicesExt[FILE_MAXNAME] = OPT_INDICES_EXT; // extension of optional file holding indices for optimizations
  char optIndicesFile[2*FILE_MAXNAME]; // optional file holding indices for optimizations
  char aggregationExt[FILE_MAXNAME] = AGGREGATION_EXT; // extension of optional file for aggregating 
  char aggregationFile[2*FILE_MAXNAME]; // optional file for aggregating 
  char paramFile[FILE_MAXNAME] = PARAM_FILE; // if, after getting optional arguments, paramFile is "", set paramFile = fileName.param
  int loc = LOC;
  double unaggedWeight = UNAGGED_WEIGHT;
  double validFrac = VALID_FRAC;
  int dataTypeIndices[MAX_DATA_TYPES]; // MAX_DATA_TYPES defined in sipnet.c
  int dataTypeSwitches[MAX_DATA_TYPES];  // 0 or 1 for each data type; used to build dataTypeIndices array
  int numDataTypes; // how many data types are we actually optimizing on?
  FILE *output;
  FILE *nullOutput; // for output we don't want to see
  char fileName[FILE_MAXNAME];
  char outFileName[FILE_MAXNAME+24];
  char climFile[FILE_MAXNAME+24]; // name of climate file
  double (*differenceFunc)(double *, OutputInfo *, int, SpatialParams *, double,
			   void (*)(double **, int, int *, SpatialParams *, int), int [], int);
  // function pointer: the difference function to use (depends on whether we're aggregating model & data) 

  // parameters used for performing the sensitivity test:
  char changeParam[PARAM_MAXNAME];
  int changeIndex; // index of parameter to change: determined dynamically from changeParam
  double lowVal, highVal; // min and max parameter values to use
  int numRuns, runNum;
  double changeAmt; // amount by which we change the param. value each run
  double paramVal; // current value
  double loglikely; // model-data error statistic

  double *sigma; // unused, but expected by likelihood function
  OutputInfo *outputInfo; // unused, but expected by likelihood function

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
  addNamelistInputItem(namelistInputs, "FILENAME", STRING_TYPE, fileName, FILE_MAXNAME);
  addNamelistInputItem(namelistInputs, "PARAM_FILE", STRING_TYPE, paramFile, FILE_MAXNAME);
  addNamelistInputItem(namelistInputs, "CHANGE_PARAM", STRING_TYPE, changeParam, PARAM_MAXNAME);
  addNamelistInputItem(namelistInputs, "LOW_VAL", DOUBLE_TYPE, &lowVal, 0);
  addNamelistInputItem(namelistInputs, "HIGH_VAL", DOUBLE_TYPE, &highVal, 0);
  addNamelistInputItem(namelistInputs, "NUM_RUNS", INT_TYPE, &numRuns, 0);
  addNamelistInputItem(namelistInputs, "LOC", INT_TYPE, &loc, 0);
  addNamelistInputItem(namelistInputs, "VALID_FRAC", DOUBLE_TYPE, &validFrac, 0);
  addNamelistInputItem(namelistInputs, "OPT_INDICES_EXT", STRING_TYPE, optIndicesExt, FILE_MAXNAME);
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
  dieIfNotSet(namelistInputs, "CHANGE_PARAM");
  dieIfNotRead(namelistInputs, "LOW_VAL");
  dieIfNotRead(namelistInputs, "HIGH_VAL");
  dieIfNotRead(namelistInputs, "NUM_RUNS");

  numDataTypes = parseDataTypeSwitches(dataTypeIndices, dataTypeSwitches);
  if (numDataTypes == 0)  {
    printf("ERROR: Must specify at least one data type to include in optimization\n");
    printf("Please modify %s and re-run\n", inputFile);
    exit(1);
  }

  // Build optIndicesFile, aggregationFile (file names):
  if (strcmp(optIndicesExt, "") != 0)
    buildFileName(optIndicesFile, fileName, optIndicesExt);
  else
    strcpy(optIndicesFile, "");

  if (strcmp(aggregationExt, "") != 0)
    buildFileName(aggregationFile, fileName, aggregationExt);
  else
    strcpy(aggregationFile, "");


  if (strcmp(paramFile, "") == 0) { // no alternative parameter file specified
    // set paramFile = {fileName}.param
    buildFileName(paramFile, fileName, "param");
  }
  buildFileName(climFile, fileName, "clim");
  buildFileName(outFileName, fileName, "senstest");


  numLocs = initModel(&spatialParams, &steps, paramFile, climFile);
  // this is the number of locations in various files, but we'll only run at one location

  changeIndex = locateParam(spatialParams, changeParam);
  if (changeIndex == -1)  {
    printf("Invalid parameter '%s'\n", changeParam);
    printf("Please fix CHANGE_PARAM in %s and re-run\n", INPUT_FILE);
    exit(1);
  }

  changeAmt = (highVal - lowVal)/(double)(numRuns - 1);

  output = openFile(outFileName, "w");
  nullOutput = tmpfile(); // for output we don't want to see

  readData(fileName, dataTypeIndices, numDataTypes, MAX_DATA_TYPES, numLocs, steps, 
	   validFrac, optIndicesFile, "", nullOutput); // 2nd to last argument is compareIndicesFile, for which we have none

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
  else // no model-data aggregation (default)
    differenceFunc = difference; // plain old "vanilla" difference function

  sigma = makeArray(numDataTypes); // unused, but expected by the likelihood function
  outputInfo = newOutputInfo(numDataTypes, loc); // unused, but expected by the likelihood function

  paramVal = lowVal;
  for (runNum = 0; runNum < numRuns; runNum++) {
    setSpatialParam(spatialParams, changeIndex, loc, paramVal); // set the parameter value
    
    // run the model, compute log likelihood (model-data error statistic)
    // paramWeight (5th argument) = 0
    loglikely = -1.0 * (*differenceFunc)(sigma, outputInfo, loc, spatialParams,
					 0, runModelNoOut, dataTypeIndices, numDataTypes);

    fprintf(output, "%f\t%f\n", paramVal, loglikely); 

    paramVal += changeAmt;
  }

  // free various pointers, close files, etc.:
  cleanupModel(numLocs);
  cleanupParamchange();
  deleteSpatialParams(spatialParams);
  free(steps);
  free(sigma);
  freeOutputInfo(outputInfo, numDataTypes);
  fclose(output); 

  return 0;
}

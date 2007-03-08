// Bill Sacks
// 2/23/05

// Edited by John Zobitz
// 3/7/07

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

// important constants - default values:

#define FILE_FOR_AGG "" // no file
#define UNAGGED_WEIGHT 0.0 // if aggregation is done, give no weight to unaggregated points in optimization
#define LOC 0 // default is run at location 0
#define PARAM_FILE "" // file to use in place of fileName.param - empty string means use fileName.param
#define OPT_INDICES_FILE "" // file giving start and end indices for optimization at each location; no file means use all points
#define VALID_FRAC 0.5 /* fraction of data points which must be valid
		     to use data from a given time step */

#define NUM_PARAMS 84		// total number of params -- this must be changed if new parameters are added!
void usage(char *progName) {
  char **dataTypeNames = getDataTypeNames(); // data types that can be used in optimization
  int i;

  printf("Usage: %s fileName numRuns typeNum1 [typeNum2 ...], where:\n", progName);
  printf("fileName.param is the file of parameter values (unless -p option is specified)\n");
  printf("fileName.param-spatial is the file of spatilly-varying parameters (first line must contain a single int: # of locations)\n");
  printf("\t(if -p option is specified, then use {paramFile}-spatial for spatially-varying parameters)\n");
  printf("fileName.clim is the file of climate data for each time step\n");
  printf("fileName.dat is the file of measured data (one column per data type)\n");
  printf("fileName.spd [Steps Per Day] contains one line per location; each line begins with year and julian day of 1st point,\n");
  printf("\tfollowed by the number of steps in each day, terminated by -1\n");
  printf("fileName.valid contains fraction of valid data points for each time step (one col. per data type)\n\n");
  printf("paramNum is the index (0-indexing) of the parameter to change in the sensitivity test\n");
  printf("parameter is varied from minVal to maxVal over a total of numRuns from the .param file\n\n");
  printf("typeNum1 (and, optionally, typeNum2 ...) specify the data type(s) to use in optimization, as follows:\n");
  for (i = 0; i < MAX_DATA_TYPES; i++)
    printf("%d: %s\n", i, dataTypeNames[i]); 
  printf("\nOutput from sensitivity test put in fileName.senstest. Each line contains: paramVal  logLikelihood\n\n");
  
  printf("The following are optional arguments, with their default values:\n");
  printf("[ -f fileforAgg ] : specify a file containing # of time steps per model/data aggregation\n");
  printf("\twhere each line contains aggregation info for one location, and each line is terminated with a -1\n");
  printf("\tEach value (i) on a given line is an integer giving the number of time steps in aggregation i in that time step\n");
  printf("\t(e.g. to run model-data comparison on a yearly aggregation, each value would be number of steps in year i)\n");
  printf("\t(if no file specified, will perform no aggregation) {%s}\n", FILE_FOR_AGG);
  printf("[ -g unAggedWeight ] : if aggregation is done, relative weight of unaggregated points in optimization\n");
  printf("\t(should probably be 0 or 1; 0 means only use aggregated points in optimization) {%f}\n", UNAGGED_WEIGHT);
  printf("[ -l loc ] : location to run at (can only perform sensitivity test at a single location) {%d}\n", LOC);
  printf("[ -p paramFile ] : instead of fileName.param, use given file for parameter values and info {%s}\n", PARAM_FILE);
  printf("[ -t opTIndicesFile ] : file giving, for each location, start and end indices for optimization\n");
  printf("\t(one line per location, with each line containing two integers: start & end; if no file specified, use all points)\n");
  printf("\t(1-indexing; end = -1 means go to end) {%s}\n", OPT_INDICES_FILE);
  printf("[ -v validFrac ] : fraction of data points which must be valid\n");
  printf("\tto use data from a given time step {%f}\n", VALID_FRAC);
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


int main(int argc, char *argv[]) {
  SpatialParams *spatialParams; // the parameters used in the model (possibly spatially-varying)
  int numLocs; /* number of spatial locations in the various files: read from spatial param. file
		  note, though, that we only perform the sensitivity test at a single location */
  int *steps; // number of time steps in each location
  char option;
  char optIndicesFile[256] = OPT_INDICES_FILE; // optional file holding indices for optimizations
  char fileForAgg[256] = FILE_FOR_AGG; // optional file for aggregating 
  char paramFile[256] = PARAM_FILE; // if, after getting optional arguments, paramFile is "", set paramFile = fileName.param
  int loc = LOC;
  double unaggedWeight = UNAGGED_WEIGHT;
  double validFrac = VALID_FRAC;
  int dataTypeIndices[MAX_DATA_TYPES]; // MAX_DATA_TYPES defined in sipnet.c
  int numDataTypes; // how many data types are we actually optimizing on?
  char *errc = "";
  FILE *outputParams;
  FILE *outputValues;
  FILE *nullOutput; // for output we don't want to see
  char fileName[256];
  char outFileNameParams[256];
  char outFileNameValues[256];
  char climFile[256]; // name of climate file
  double (*differenceFunc)(double *, OutputInfo *, int, SpatialParams *, double,
			   void (*)(double **, int, int *, SpatialParams *, int), int [], int);
  // function pointer: the difference function to use (depends on whether we're aggregating model & data) 

  numDataTypes = initDataTypeIndices(dataTypeIndices); // initialize array to [0,1,...,MAX_DATA_TYPES-1] (default)

  // parameters used for performing the sensitivity test:
  int paramNum; // index of parameter to change
  double minVal, maxVal; // min and max parameter values to use
  int numRuns, runNum;
  double changeAmt; // amount by which we change the param. value each run
  double paramVal; // current value
  double loglikely; // model-data error statistic

  double *sigma; // unused, but expected by likelihood function
  OutputInfo *outputInfo; // unused, but expected by likelihood function

  int paramStep;	// index of params

  // get command-line arguments:
  while ((option = getopt(argc, argv, "f:g:l:p:t:v:")) != -1) { 
    // we have another optional argument
    switch(option) {
    case 'f':
      strcpy(fileForAgg, optarg);
      break;
    case 'g':
      unaggedWeight = strtod(optarg, &errc);
      break;
    case 'l':
      loc = strtol(optarg, &errc, 0);
      break;
    case 'p':
      strcpy(paramFile, optarg);
      break;
    case 't':
      strcpy(optIndicesFile, optarg);
      break;
    case 'v':
      validFrac = strtod(optarg, &errc);
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

  if (argc < (optind + 2)) { // expecting at least 2 more arguments, but there are fewer than that
    usage(argv[0]);
    exit(1);
  }

  // read remaining command-line arguments:
  strcpy(fileName, argv[optind]);
  if (strcmp(paramFile, "") == 0) { // no alternative parameter file specified
    // set paramFile = {fileName}.param
    strcpy(paramFile, fileName);
    strcat(paramFile, ".param");
  }
  strcpy(climFile, fileName);
  strcat(climFile, ".clim");
  
  // Each row of the out file is a separate parameter name
  strcpy(outFileNameParams, fileName);
  strcat(outFileNameParams, ".Params.senstest");
  strcpy(outFileNameValues, fileName);
  strcat(outFileNameValues, ".Values.senstest");



  numRuns = strtol(argv[optind+1], &errc, 0);
  numDataTypes = parseDataTypeIndices(dataTypeIndices, argv, optind+2, argc);


  outputParams = openFile(outFileNameParams, "w");
  outputValues = openFile(outFileNameValues, "w");
  nullOutput = tmpfile(); // for output we don't want to see

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
  else // no model-data aggregation (default)
    differenceFunc = difference; // plain old "vanilla" difference function



// Loop through all the parameters
for (paramStep=0; paramStep < NUM_PARAMS; paramStep++) {

	// Set up the model over again, read data, etc   (if this is outside the for loop then we are using
	// old parameter values again)
	
 	numLocs = initModel(&spatialParams, &steps, paramFile, climFile);

  	readData(fileName, dataTypeIndices, numDataTypes, MAX_DATA_TYPES, numLocs, steps, 
	   	validFrac, optIndicesFile, "", nullOutput); // 2nd to last argument is compareIndicesFile, for which we have none


  	sigma = makeArray(numDataTypes); // unused, but expected by the likelihood function
  	outputInfo = newOutputInfo(numDataTypes, loc); // unused, but expected by the likelihood function


 	paramNum=paramStep;

 
  	minVal = getSpatialParamMin(spatialParams, paramNum);
  	maxVal = getSpatialParamMax(spatialParams, paramNum);

  	changeAmt = (maxVal - minVal)/(double)(numRuns - 1);

	// Verify that with each iteration we are resetting the parameters.  When this is uncommented
	// then the value printed on the screen should be the parameter value in niwots.param in the
	// second column
	
/*	if (paramStep>0) {
		printf("%f\n",getSpatialParam(spatialParams,paramNum,loc)); }
*/ 
  	paramVal = minVal;
  	for (runNum = 0; runNum < numRuns; runNum++) {
    	setSpatialParam(spatialParams, paramNum, loc, paramVal); // set the parameter value
    
    	// run the model, compute log likelihood (model-data error statistic)
    	// paramWeight (5th argument) = 0
    	loglikely = -1.0 * (*differenceFunc)(sigma, outputInfo, loc, spatialParams,
					 0, runModelNoOut, dataTypeIndices, numDataTypes);

		// Print the parameter and likelihood values
    	fprintf(outputParams, "%f\t", paramVal); 
		fprintf(outputValues, "%8.3f\t", loglikely);
	
    	paramVal += changeAmt;
  	}

    fprintf(outputParams, "\n"); 
	fprintf(outputValues, "\n");


  	// free various pointers, (this needs to be done every time we change parameters)
  	cleanupModel(numLocs);
  	cleanupParamchange();
  	deleteSpatialParams(spatialParams);
  	free(steps);
  	free(sigma);
  	freeOutputInfo(outputInfo, numDataTypes);
  
}
	// close file
  fclose(outputParams); 
  fclose(outputValues);
  return 0;
}

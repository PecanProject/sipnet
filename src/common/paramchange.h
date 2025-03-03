
#ifndef PARAMCHANGE_H
#define PARAMCHANGE_H

#include "spatialParams.h"

// used for parameter estimation
typedef struct ChangeableParamInfo {
  char name[64];
  double guess;
  double min;
  double max;
  double sigma;
} PInfo;

typedef struct OutputInfoStruct {
  double meanError; // mean error per day
  double daysError; // daily-aggregated mean error per day
  double *years; // for each year, total for the year
  int numYears; // size of years array
} OutputInfo;



/* Difference, version 3 - ESTIMATES SIGMA, RETURNS AGGREGATE INFO
   Run modelF with given parameters at location loc, compare output with measured data
   using data types given by dataTypeIndices[0..numDataTypes-1]
   Return a measure of the difference between measured and predicted data
   (Higher = worse)
   Return best sigma value for each data type in sigma[0..numDataTypes-1]
   Return mean error, mean daily-aggregated error, and yearly-aggregated output for each year
   and each data type in *outputInfo array
   Pre: sigma and outputInfo are already malloced, as are outputInfo[*].years arrays

   Only use "valid" data points (as determined by validFrac in readData)
   And only use points between startOpt and endOpt (set in readData)
   [IGNORE paramWeight - just there to be consistent with old difference function]

   NOTE: this is actually the NEGATIVE log likelihood, discarding constant terms
   to get true log likelihood, add n*log(sqrt(2*pi)), then multiply by -1
*/
double difference(double *sigma, OutputInfo *outputInfo,
		  int loc, SpatialParams *spatialParams, double paramWeight,
		  void (*modelF)(double **, int, int *, SpatialParams *, int),
		  int dataTypeIndices[], int numDataTypes, int costFunction, double dataTypeWeights[]);



/* Aggregated difference - ESTIMATES SIGMA, RETURNS AGGREGATE INFO
   Same as difference function above, but aggregates model output to fewer steps
   Total difference is a weighted sum of error on aggregated output vs. data
   and error on unaggregated output vs. data (weight determined by global unaggedWeight)

   Run modelF with given parameters at location loc, compare output with measured data
   using data types given by dataTypeIndices[0..numDataTypes/2-1]
   Return a measure of the difference between measured and predicted data
   (Higher = worse)
   Assumes error is the same on each aggregated time step,
   even if aggregation lengths are different
   Return best sigma value for each data type in sigma[0..numDataTypes-1]
   (where sigma[i] is sigma for data type given by dataTypeIndices[i%(numDataTypes/2)];
   if i < numDataTypes/2 then sigma[i] gives sigma for unaggregated data; otherwise for aggregated data)
   Return mean error, mean daily-aggregated error, and yearly-aggregated output for each year
   and each data type in *outputInfo array (indexed like sigma array - see above)

   Pre: sigma and outputInfo are already malloced, as are outputInfo[*].years arrays
   global *numAggSteps, **aggSteps and ***aggedData have all been set appropriately,
   and global **aggedModel has been malloced appropriately
   numDataTypes is actually TWICE the number of data types
   (for each data type, one unaggregated and one aggregated)

   FOR NOW, WE IGNORE THE VALID FRACTION (THIS HASN'T BEEN AGGREGATED UP)
   Only use points between startOpt and endOpt (set in readData)
   (NOTE: UNTESTED FOR ANYTHING BUT STARTOPT=1, ENDOPT=-1 (I.E. ALL POINTS))
   [IGNORE paramWeight - just there to be consistent with difference function above]
*/

// 6/9/11: For now we don't do anything with the cost function or data Type weights - we just want to make
// it consistent with the difference function above so we don't get an error.

double aggedDifference(double *sigma, OutputInfo *outputInfo,
		       int loc, SpatialParams *spatialParams, double paramWeight,
		       void (*modelF)(double **, int, int *, SpatialParams *, int),
		       int dataTypeIndices[], int numDataTypes, int costFunction, double dataTypeWeights[]);

/* Take array of model output, compare output with measured data - for given dataNum
   (i.e. perform comparisons between model[*][dataNum] and data[loc][*][dataNum]
   Return (in outputInfo[dataNum]) mean error (per day), mean daily-aggregated error (per day)
   Also fill years array with annual total NEE for each year
   and set outputInfo.numYears, the size of the years array
   Pre: outputInfo is already malloced, as are outputInfo[*].years arrays
   (Can later use years array to find yearly-aggregated error and interannual variability)
*/
void aggregates(OutputInfo *outputInfo, double **model, int loc, int dataNum);



/* Read measured data (from fileName.dat) and valid fractions (from fileName.valid) into arrays (used to also read sigmas)
   and set values in valid array (based on validFrac)
   Each line in data (and valid) file has totNumDataTypes columns
   and each file has one line for each time step at each of the myNumLocs location (all time steps for a single location are continuous),
   with NO blank lines
   where steps vector gives # of time steps at each location (so total number of lines in data/valid files should equal sum of elements in steps vector)
   dataTypeIndices give the numDataTypes indices of the data types that we'll actually use in optimization
   Also read spd file (steps per day) (fileName.spd), which has one line for each location
   Each line begins with the year of the first point, followed by the julian day of the first point,
   followed by the number of steps per day for each day, terminated by -1
   As a short-hand, #n means repeat the last steps-per-day value n more times (e.g. "2 #3" is equivalent to "2 2 2 2")

   Also possibly read start and end indices for optimizations for each location (from optIndicesFile),
   and start and end indices for final model-data comparisons for each location (from compareIndicesFile),
   where these files have one line for each location, and each line contains two integers: start & end;
   if either of these file names is given as an empty string (""), use all data points for optimizations or model-data comparisons.
   NOTE: Both of these use 1-indexing; end = -1 means go to end

   steps is an array giving the number of time steps at each of the myNumLocs locations

   This function also allocates space for model array
*/
void readData(char *fileName, int dataTypeIndices[], int numDataTypes, int totNumDataTypes, int myNumLocs, int *steps,
	      double validFrac, char *optIndicesFile, char *compareIndicesFile, FILE *outFile);


/* pre: readData has been called (to set global startOpt, endOpt and numLocs appropriately)

   read number of time steps per each model-data aggregation from file
   each line in fileForAgg contains aggregation info for one location,
   where each value (i) on a given line is an integer giving the number of time steps in aggregation i in that time step
   (e.g. to run model-data comparison on a yearly aggregation, each value would be number of steps in year i);
   each line must be terminated with a -1

   if sum of all time steps (i.e. sum of all numbers on line) differs from total number of data,
   exit with an error message

   otherwise,
   set global numAggSteps (1-d array: spatial) and aggSteps (2-d array: spatial) and unaggedWeight appropriately
   also compute aggedData array (of size numLocs x numAggSteps x numDataTypes)
*/
void readFileForAgg(char *fileForAgg, int numDataTypes, double myUnaggedWeight);


// malloc space for outputInfo array[0..numDataTypes-1], and outputInfo[*].years arrays for a single location, loc
// make years arrays large enough to hold data from given location
OutputInfo *newOutputInfo(int numDataTypes, int loc);


// copy data from outputInfo ptr in to outputInfo ptr out
// PRE: outputInfo struct pointed to by out has already been malloced, as has its years array
void copyOutputInfo(OutputInfo *out, OutputInfo *in);

// given an outputInfo array[0..numDataTypes-1] (dynamically allocated), free years and the array itself
void freeOutputInfo(OutputInfo *outputInfo, int numDataTypes);


// call this when done program:
// free space used by global pointers
void cleanupParamchange();




#endif

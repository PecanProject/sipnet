// header file for sipnet



#ifndef SIPNET_H
#define SIPNET_H

#include <stdio.h>
#include "spatialParams.h"

#define EXTRA_DATA_TYPES 0
// Allow the outputting of extra data types (e.g. GPP, Rtot)?
// NOTE: Defining this flag as true will break the estimate program, since there will not be enough columns in the .dat file
//  However, this can be used for outputting additional data types when computing means and standard deviations across a number of parameter sets

// number of different possible data types that can be output - depends on how EXTRA_DATA_TYPES is defined
#if EXTRA_DATA_TYPES
#define MAX_DATA_TYPES 20
#else
#define MAX_DATA_TYPES 3
#endif 


// write to file which model components are turned on
// (i.e. the value of the #DEFINE's at the top of file)
// pre: out is open for writing
void printModelComponents(FILE *out);


// return an array[0..MAX_DATA_TYPES-1] of strings,
// where arr[i] gives the name of data type i
char **getDataTypeNames();


/* do initializations that only have to be done once for all model runs:
   read in climate data and initial parameter values
   parameter values get stored in spatialParams (along with other parameter information),
   which gets allocated and initialized here (thus requiring that spatialParams is passed in as a pointer to a pointer)
   number of time steps in each location gets stored in steps vector, which gets dynamically allocated with malloc
   (steps must be a pointer to a pointer so it can be malloc'ed)

   also set up pointers to different output data types
   and setup meanNPP tracker

   initModel returns number of spatial locations

   paramFile is parameter data file
   paramFile-spatial is file with parameter values of spatially-varying parameters (1st line contains number of locations)
   climFile is climate data file
*/
int initModel(SpatialParams **spatialParams, int **steps, char *paramFile, char *climFile, char *optionsFile);


// call this when done running model:
// de-allocates space for climate linked list
// (needs to know number of locations)
void cleanupModel(int numLocs);


/* pre: outArray has dimensions of at least (# model steps) x numDataTypes
   dataTypeIndices[0..numDataTypes-1] gives indices of data types to use (see DATA_TYPES array in sipnet.h)
   
   run model with parameter values in spatialParams, don't output to file
   instead, output some variables at each step into an array
   Run at spatial location given by loc (0-indexing)
   Note: can only run at one location: to run at all locations, must put runModelNoOut call in a loop
*/
void runModelNoOut(double **outArray, int numDataTypes, int dataTypeIndices[], SpatialParams *spatialParams, int loc);


/* do one run of the model using parameter values in spatialParams, output results to out
   If printHeader = 1, print a header for the output file, if 0 don't
   Run at spatial location given by loc (0-indexing) - or run everywhere if loc = -1
   Note: number of locations given in spatialParams
*/
void runModelOutput(FILE *out, int printHeader, SpatialParams *spatialParams, int loc);


// do a sensitivity test on paramNum, varying from low to high, doing a total of numRuns runs
// run only at a single location (given by loc)
void sensTest(FILE *out, int paramNum, double low, double high, int numRuns, SpatialParams *spatialParams, int loc); 


#endif

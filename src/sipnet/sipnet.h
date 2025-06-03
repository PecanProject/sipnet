// header file for sipnet

#ifndef SIPNET_H
#define SIPNET_H

#include <stdio.h>
#include "common/spatialParams.h"
#include "outputItems.h"

// write to file which model components are turned on
// (i.e. the value of the #DEFINE's at the top of file)
// pre: out is open for writing
void printModelComponents(FILE *out);

/* do initializations that only have to be done once for all model runs:
   read in climate data and initial parameter values
   parameter values get stored in spatialParams (along with other parameter
   information), which gets allocated and initialized here (thus requiring that
   spatialParams is passed in as a pointer to a pointer) number of time steps in
   each location gets stored in steps vector, which gets dynamically allocated
   with malloc (steps must be a pointer to a pointer so it can be malloc'ed)

   also set up pointers to different output data types
   and setup meanNPP tracker

   initModel returns number of spatial locations

   paramFile is parameter data file
   paramFile-spatial is file with parameter values of spatially-varying
   parameters (1st line contains number of locations)
   climFile is climate data file
*/
int initModel(SpatialParams **spatialParams, int **steps, char *paramFile,
              char *climFile);

/*
 * Read in event data for all the model runs
 *
 * Read in event data from a file with the following specification:
 * - one line per event
 * - all events are ordered first by location (ascending) and then by year/day
 * (ascending)
 *
 * @param eventFile Name of file containing event data
 * @param numLocs Number of locations in the event file.
 */
void initEvents(char *eventFile, int numLocs, int printHeader);

// call this when done running model:
// de-allocates space for climate linked list
// (needs to know number of locations)
void cleanupModel(int numLocs);

/* Do one run of the model using parameter values in spatialParams
   If out != NULL, output results to out
   If printHeader = 1, print a header for the output file, if 0 don't
   If outputItems != NULL, do additional outputting as given by this
     structure (1 variable per file)
   If loc == -1, then print currLoc as first item on each line
   Run at spatial location given by loc (0-indexing) - or run everywhere if loc
   = -1 Note: number of locations given in spatialParams
*/
void runModelOutput(FILE *out, OutputItems *outputItems, int printHeader,
                    SpatialParams *spatialParams, int loc);

/* PRE: outputItems has been created with newOutputItems

   Setup outputItems structure
   Each variable added will be output in a separate file ('*.varName')
 */
void setupOutputItems(OutputItems *outputItems);

#endif

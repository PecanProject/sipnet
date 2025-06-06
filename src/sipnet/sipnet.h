// header file for sipnet

#ifndef SIPNET_H
#define SIPNET_H

#include <stdio.h>
#include "common/modelParams.h"
#include "outputItems.h"

// write to file which model components are turned on
// (i.e. the value of the #DEFINE's at the top of file)
// pre: out is open for writing
void printModelComponents(FILE *out);

/*!
 * Do model initializations
 *
 * Read in initial parameter values and climate data. Also set up pointers to
 * different output data types and setup meanNPP tracker.
 *
 * @param modelParams pointer to ModelParams struct, will be alloc'd here
 * @param paramFile name of parameter file
 * @param climFile name of climate file
 */
void initModel(ModelParams **modelParams, const char *paramFile,
               const char *climFile);

/*!
 * Read in event data for all the model runs
 *
 * Read in event data from a file with the following specification:
 * - one line per event
 * - all events are ordered by year/day ascending
 *
 * @param eventFile Name of file containing event data
 */
void initEvents(char *eventFile, int printHeader);

/*!
 * Free allocated memory
 *
 * Call this when done running model
 */
void cleanupModel(void);

/*! Run the model using parameter values in modelParams
 *
 * @param out File pointer for main output; can be null to suppress this
 * @param outputItems OutputItems struct used for individual output param files
 *                    Can be null to suppress this output
 * @param printHeader Whether to print a header row in output and events.out
 */
void runModelOutput(FILE *out, OutputItems *outputItems, int printHeader);

/*!
   Setup outputItems structure

   Each variable added will be output in a separate file ('*.varName')

   @param outputItems OutputItems struct to be filled out; must be created with
                      newOutputItems()
 */
void setupOutputItems(OutputItems *outputItems);

#endif

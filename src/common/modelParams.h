#ifndef MODEL_PARAMS_H
#define MODEL_PARAMS_H

#include <stdio.h>

#define MODEL_PARAM_MAXNAME 64
#define OBSOLETE_PARAM (-1)

// struct to hold a single param
typedef struct OneModelParamStruct {
  char name[MODEL_PARAM_MAXNAME];  // name of parameter
  int isRequired;  // 1|0|OBSOLETE_PARAM (-1)
                   //  1: required param, terminate if not read
                   //  0: optional param
                   // -1: obsolete param, warn if read
  double *value;  // a pointer to this param's (external location) value
  int isRead;  // whether this param has been read
} OneModelParam;

typedef struct ModelParamsStruct {
  OneModelParam *params;  // vector of parameters, dynamically-allocated
                          // (stored in order in which they were initialized)
  int maxParams;  // maximum number of parameters
  int numParams;  // actual number of parameters (tracks # that have been
                  // initialized with initializeOneSpatialParam)
  int numParamsRead;  // tracks number of parameters that have been read in from
                      // file
  int *readIndices;  // indices (in parameters vector) of the parameters read
                     // from file, in the order in which they were read (this
                     // will allow us to output parameters in the same order
                     // later)
} ModelParams;

// allocate space for a new spatialParams structure, return a pointer to it
ModelParams *newModelParams(int maxParameters);

/* Initialize next parameter:
   Set name of parameter equal to "name",
    and set parameter's value pointer to point to the location given by
   "externalLoc" parameter (required to be non-NULL)
   Also set parameter's isRequired value: if true, then we'll terminate
   execution if this parameter is not read from file
*/
void initializeOneModelParam(ModelParams *params, char *name,
                             double *externalLoc, int isRequired);

/* Read all parameters from file, setting all values in Params structure
   appropriately

   Structure of paramFile:
      name  value

   The order of the parameters in paramFile does not matter.

   ! is a comment character in paramFile: anything after a ! on a line is
   ignored

   PRE: paramFile is open and file pointer points to start of file

   NOTE: for compatibility with the prior format, the following structure is
   also acceptable: name  value  changeable  min  max  sigma When encountered,
   the extra columns are ignored and a warning is generated.

 */
void readModelParams(ModelParams *params, FILE *paramFile);

// Return numParameters, the actual number of parameters that have been
// initialized with initializeOneSpatialParam
int getNumModelParams(ModelParams *params);

// Return number of parameters that have been read in from file
int getNumModelParamsRead(ModelParams *params);

// Find parameter with given name in the parameters vector
// If found, return index in vector, otherwise return -1
int locateParam(ModelParams *params, char *name);

// Return 1 if parameter i has had its value set, 0 otherwise
int valueSet(ModelParams *params, int i);

/* Check to see whether value is within allowable range of parameter i
   Return 1 if okay, 0 if bad
*/
int checkParam(ModelParams *params, int i, double value);

/* Return value of parameter i
   PRE: 0 <= i < modelParams->numParameters
*/
double getParam(ModelParams *params, int i);

// Clean up: deallocate modelParams and any other dynamically-allocated
// pointers that need deallocating
void deleteModelParams(ModelParams *params);

#endif

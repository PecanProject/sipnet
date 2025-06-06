#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "modelParams.h"
#include "util.h"

// Private/helper functions: not defined in modelParams.h

// return 1 if all maxParams parameters have been initialized, 0 if not
int allParamsInitialized(ModelParams *ModelParams) {
  if (ModelParams->numParams == ModelParams->maxParams) {
    // all parameters have been initialized
    return 1;
  }

  return 0;
}

// Set array[0..length-1] to all be equal to value
void setAll(double *array, int length, double value) {
  int i;

  for (i = 0; i < length; i++) {
    array[i] = value;
  }
}

// Checks to make sure that all parameters with isRequired=true have been read
// If not, kills program
// Writes out names of all parameters that weren't read (even if not required,
// as a warning message)
void checkAllRead(ModelParams *ModelParams) {
  int i;
  int okay;
  OneModelParam *param;

  okay = 1;  // so far so good
  for (i = 0; i < ModelParams->numParams; i++) {
    param = &(ModelParams->params[i]);
    if (param->value == NULL) {
      if (param->isRequired) {  // should have been read but wasn't!
        okay = 0;
        printf("ERROR: Didn't read required parameter %s\n", param->name);
      } else {
        printf("WARNING: Didn't read parameter %s (not flagged as required, so "
               "continuing)\n",
               param->name);
      }
    }
  }

  if (!okay) {
    exit(1);
  }
}

/*************************************************/

// Public functions: defined in ModelParams.h

// allocate space for a new ModelParams structure, return a pointer to it
ModelParams *newModelParams(int maxParams) {
  ModelParams *modelParams;

  modelParams = (ModelParams *)malloc(sizeof(ModelParams));
  modelParams->maxParams = maxParams;
  modelParams->numParams = 0;  // for now, anyway
  modelParams->numParamsRead = 0;  // for now, anyway

  // allocate space for vectors within this structure:
  modelParams->params =
      (OneModelParam *)malloc(maxParams * sizeof(OneModelParam));
  modelParams->readIndices = (int *)malloc(maxParams * sizeof(int));  // the
                                                                      // biggest
                                                                      // it
                                                                      // could
                                                                      // be

  return modelParams;
}

void initializeOneModelParam(ModelParams *modelParams, char *name,
                             double *externalLoc, int isRequired) {
  int paramIndex;  // index of next uninitialized parameter
  OneModelParam *param;  // a pointer to the next uninitialized parameter

  if (allParamsInitialized(modelParams)) {  // all parameters have been
                                            // initialized!
    printf("Error trying to initialize %s: have already initialized all %d "
           "parameters\n",
           name, modelParams->maxParams);
    printf("Check value of maxParams passed into newModelParams function\n");
    exit(1);
  }

  // otherwise, get the index of the next uninitialized parameter
  paramIndex = modelParams->numParams;
  // and set param to point to it for easier access
  param = &(modelParams->params[paramIndex]);

  strcpy(param->name, name);
  param->value = externalLoc;
  param->isRequired = isRequired;

  modelParams->numParams++;
}

void checkParamFormat(char *line, const char *sep) {
  int numParams = countFields(line, sep);
  if (numParams > 2) {
    printf("WARNING: extra columns in .param file are being ignored (found %d "
           "columns)\n",
           numParams);
  }
}

void readModelParams(ModelParams *modelParams, FILE *paramFile) {
  const char *SEPARATORS = " \t\n\r";  // characters that can separate values in
                                       // parameter files
  const char *COMMENT_CHARS = "!";  // comment characters (ignore everything
                                    // after this on a line)

  char line[256];
  char pName[MODEL_PARAM_MAXNAME];  // parameter name
  int paramIndex;
  OneModelParam *param;  // a pointer to a single parameter, for easier access
  char strValue[32];  // before we know whether value is a number or "*"
  double value;
  char *errc;
  int isComment;

  // Check for old-style (spatial param) format on first line containing params
  int formatChecked = 0;

  while (fgets(line, sizeof(line), paramFile) != NULL) {  // while not EOF or
                                                          // error
    // remove trailing comments:
    isComment = stripComment(line, COMMENT_CHARS);

    if (!isComment) {  // if this isn't just a comment line or blank line
      // Check for old spatial-param format and warn if appropriate
      if (!formatChecked) {
        checkParamFormat(line, SEPARATORS);
        formatChecked = 1;
      }

      // tokenize line:
      strcpy(pName, strtok(line, SEPARATORS));  // copy first token into pName

      strcpy(strValue, strtok(NULL, SEPARATORS));
      value = strtod(strValue, &errc);
      paramIndex = locateParam(modelParams, pName);

      if (paramIndex == -1) {  // not found
        printf("WARNING: Ignoring parameter %s: this parameter wasn't "
               "initialized in the code\n",
               pName);
      } else if (valueSet(modelParams, paramIndex)) {
        printf("Error reading parameter file: read %s, but this parameter has "
               "already been set\n",
               pName);
        exit(1);
      } else {  // otherwise, we're good to go
        // set param to point to the appropriate parameter, for easier access
        param = &(modelParams->params[paramIndex]);
        *(param->value) = value;
        param->isRead = 1;
        // mark this as the next parameter read from file:
        modelParams->readIndices[modelParams->numParamsRead] = paramIndex;
        modelParams->numParamsRead++;
      }  // else (no errors in reading this line from parameter file)
    }  // if !isComment
  }  // while not EOF or error

  // check for error in reading:
  if (ferror(paramFile)) {
    printf("Error reading file in readModelParams\n");
    printf("ferror = %d\n", ferror(paramFile));
    exit(1);
  }

  checkAllRead(modelParams);  // terminate program if some required parameters
                              // weren't read
}  // readModelParams

// Return numParams, the actual number of parameters that have been
// initialized with initializeOneModelParam
int getnumParams(ModelParams *modelParams) { return modelParams->numParams; }

// Return number of parameters that have been read in from file
int getNumParamsRead(ModelParams *modelParams) {
  return modelParams->numParamsRead;
}

// Find parameter with given name in the parameters vector
// If found, return index in vector, otherwise return -1
int locateParam(ModelParams *modelParams, char *name) {
  int i;
  int found;

  i = 0;
  found = 0;
  while (i < modelParams->numParams && !found) {
    if (strcmpIgnoreCase(name, modelParams->params[i].name) == 0) {
      found = 1;
    }
    i++;
  }

  if (found) {
    return (i - 1);
  }

  return -1;
}

// Return 1 if parameter i has had its value set, 0 otherwise
int valueSet(ModelParams *modelParams, int i) {
  return modelParams->params[i].isRead;
}

void deleteModelParams(ModelParams *modelParams) {
  // Free space used by ModelParams structure itself:
  free(modelParams->params);
  free(modelParams->readIndices);
  free(modelParams);
}

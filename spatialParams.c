/* spatialParams: structure and functions to track and manipulate parameters that may vary spatially
   
   Author: Bill Sacks
   
   Creation date: 8/13/04
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "spatialParams.h" // includes definition of SpatialParams structure, as well as OneSpatialParam structure
#include "util.h"


// Private/helper functions: not defined in spatialParams.h

/* return index of next unread parameter
   if no more unread parameters, returns spatialParams->numParameters
*/
int nextUnreadParam(SpatialParams *spatialParams) {
  int paramIndex;
 
  paramIndex = 0;
  while (paramIndex < spatialParams->numParameters && spatialParams->parameters[paramIndex].value != NULL)
    // while we haven't reached the end of the parameters vector, and we haven't found an unitialized parameter
    paramIndex++;

  return paramIndex;
}


// Set array[0..length-1] to all be equal to value
void setAll(double *array, int length, double value) {
  int i;

  for (i = 0; i < length; i++)
    array[i] = value;
}


/* If isSpatial = 0, return array[0] (ignore loc)
   Otherwise, return array[loc]
*/
double getPossiblySpatial(double *array, int loc, int isSpatial) {
  if (isSpatial)
    return array[loc];
  else
    return array[0];
}


/* If isSpatial = 0, set array[0] = value (ignore loc)
   Otherwise, if loc = -1, set array[0..length-1] = value
   Otherwise, set array[loc] = value
*/
void setPossiblySpatial(double *array, int loc, double value, int isSpatial, int length) {
  if (isSpatial) {
    if (loc == -1)
      setAll(array, length, value);
    else
      array[loc] = value;
  }
  else // non-spatial
    array[0] = value;
}

/*************************************************/

// Public functions: defined in spatialParams.h


// allocate space for a new spatialParams structure, return a pointer to it
SpatialParams *newSpatialParams(int numParameters, int numLocs) {
  SpatialParams *spatialParams;
  int i;

  spatialParams = (SpatialParams *)malloc(sizeof(SpatialParams));
  spatialParams->numParameters = numParameters;
  spatialParams->numLocs = numLocs;
  spatialParams->numChangeableParams = 0; // for now, anyway
  
  // allocate space for vectors within this structure:
  spatialParams->parameters = (OneSpatialParam *)malloc(numParameters * sizeof(OneSpatialParam));
  spatialParams->changeableParamIndices = (int *)malloc(numParameters * sizeof(int)); // the biggest it could have to be

  // set value pointers of all parameters to be NULL (this will mark uninitialized parameter):
  for (i = 0; i < numParameters; i++)
    spatialParams->parameters[i].value = NULL;

  return spatialParams;
}


// return 1 if all parameters have been read in, 0 if not
int allParamsRead(SpatialParams *spatialParams) {
  if (nextUnreadParam(spatialParams) == spatialParams->numParameters) // all parameters have been read
    return 1; 
  else
    return 0;
}
  

/* Read next parameter from file, setting all values in spatialParams structure appropriately
   Ensure that name is the name of the parameter we're reading
   Set this parameter's externalLoc pointer to point to the location given by the externalLoc parameter
   Note: acceptable to have externalLoc = NULL, but then (obviously) won't be assigned by loadSpatialParams

   Structure of paramFile:
      name  value  changeable  min  max  sigma
   where value is a number if parameter is non-spatial, or * if parameter is spatial; changeable is 0 or 1 (boolean)
   If parameter is spatial, look in spatialParamFile for value, which has the following structure:
      name  value  [value  value...]
   where the number of values is equal to the number of spatial locations

   PRE: paramFile is open and file pointer points to the next parameter to be read
        if this is a spatial parameter, spatialParamFile is open and file pointer points to the next parameter to be read
*/
void readOneSpatialParam(SpatialParams *spatialParams, FILE *paramFile, FILE *spatialParamFile, char *name, double *externalLoc) {
  const char *TOKENS = " \t"; // tokens that can separate values in spatialParams file (space & tab)
  
  char line[256];
  char pName[64]; // parameter name
  char strValue[32]; // before we know whether value is a number or "*"
  double value, min, max, sigma;
  int changeable;
  char *errc;
  int status;
  int paramIndex; // index of next uninitialized parameter
  OneSpatialParam *param; // a pointer to the next uninitialized parameter
  int numLocs, i;

  if (allParamsRead(spatialParams)) { // all parameters have been read
    printf("Error trying to read %s: no more uninitialized parameters to fill\n", name);
    exit(1);
  }

  // otherwise, get the index of the next uninitialized parameter
  paramIndex = nextUnreadParam(spatialParams);
  // and set param to point to it for easier access
  param = &(spatialParams->parameters[paramIndex]);

  // read next line:
  if (fgets(line, sizeof(line), paramFile) == NULL) {
    printf("Error trying to read %s: error or unexpected EOF in parameter file\n", name);
    printf("feof = %d, ferror = %d\n", feof(paramFile), ferror(paramFile));
    exit(1);
  }

  // tokenize line:
  strcpy(pName, strtok(line, TOKENS)); // copy first token into pName
  if (strcasecmp(pName, name) !=0) { // compare, ignore case
    printf("Error: read %s, expected %s\n", pName, name);
    exit(1);
  }

  // continue to tokenize:
  strcpy(strValue, strtok(NULL, TOKENS)); // copy next token into strValue; wait until later to figure out if it's "*" or a number
  changeable = strtol(strtok(NULL, TOKENS), &errc, 0);
  min = strtod(strtok(NULL, TOKENS), &errc);
  max = strtod(strtok(NULL, TOKENS), &errc);
  sigma = strtod(strtok(NULL, TOKENS), &errc);
  
  // fill the new spatialParam structure with everything but numLocs, value, guess, best, and knob (which we'll do later)
  strcpy(param->name, pName);
  param->isChangeable = changeable;
  param->externalLoc = externalLoc;
  param->min = min;
  param->max = max;
  param->sigma = sigma;

  if (changeable) { // we need to update info on changeable params in spatialParams
    spatialParams->changeableParamIndices[spatialParams->numChangeableParams] = paramIndex;
    // before we change it, spatialParams->numChangeableParams is the index of the first free spot in the changeableParamIndices vector
    spatialParams->numChangeableParams++;
  }
  
  // now we need to see if we read in an actual value, or a "*" (if the latter, it's a spatially-varying parameter)
  if (strcmp(strValue, "*") == 0) { // we read "*": this is a spatially-varying parameter
    numLocs = param->numLocs = spatialParams->numLocs;

    // read info from spatialParamFile:
    status = fscanf(spatialParamFile, "%s", pName);
    if (status == EOF || status == 0) { // error reading
      printf("Error trying to read %s from spatialParamFile\n", name);
      exit(1);
    }
    else if (strcasecmp(pName, name) != 0) { // compare, ignore case
      printf("Error: read %s from spatialParamFile, expected %s\n", pName, name);
      exit(1);
    }

    // we've read correct name, now read all the values:
    // first, allocate space:
    param->value = (double *)malloc(numLocs * sizeof(double));
    param->guess = (double *)malloc(numLocs * sizeof(double));
    param->best = (double *)malloc(numLocs * sizeof(double));
    param->knob = (double *)malloc(numLocs * sizeof(double));

    // now read values and assign param->value, guess, best, and knob:
    for (i = 0; i < numLocs; i++) {
      status = fscanf(spatialParamFile, "%lf", &value);
      if (status == EOF || status == 0) { // error reading
	printf("Error: did not find enough values in spatialParamFile for %s\n", pName);
	exit(1);
      }

      // assign value, guess and best to all be the guess value initially:
      param->value[i] = param->guess[i] = param->best[i] = value;
      param->knob[i] = 0; // assign knob to be 0 initially
    } // for i
  } // if spatially-varying
  else { // non-spatially-varying parameter
    param->numLocs = 0; // signifies non-spatially-varying
    
    // allocate space for a single value in each of param->value, guess, best and knob:
    param->value = (double *)malloc(sizeof(double));
    param->guess = (double *)malloc(sizeof(double));
    param->best = (double *)malloc(sizeof(double));
    param->knob = (double *)malloc(sizeof(double));

    // assign value, guess and best to all be the guess value initially:
    value = strtod(strValue, &errc); // convert string value to double
    param->value[0] = param->guess[0] = param->best[0] = value;
    param->knob[0] = 0; // assign knob to be 0 initially
  } // else non-spatially-varying
  
} // readOneSpatialParam


/* Return 1 if parameter i varies spatially, 0 if not
   PRE: 0 <= i < spatialParams->numParameters
*/
int isSpatial(SpatialParams *spatialParams, int i) {
  return (spatialParams->parameters[i].numLocs > 0);
}


/* Return 1 if parameter i is changeable, 0 if not
   PRE: 0 <= i < spatialParams->numParameters 
*/
int isChangeable(SpatialParams *spatialParams, int i) {
  return spatialParams->parameters[i].isChangeable;
}


/* Check to see whether value is within allowable range of parameter i
   Return 1 if okay, 0 if bad
*/
int checkSpatialParam(SpatialParams *spatialParams, int i, double value) {
  return (value >= getSpatialParamMin(spatialParams, i) && value <= getSpatialParamMax(spatialParams, i));
}


/* Return value of parameter i, location loc (if this parameter is non-spatial, ignore loc) 
   PRE: 0 <= i < spatialParams->numParameters
        if this parameter is spatial, then 0 <= loc < spatialParams->numLocs
*/
double getSpatialParam(SpatialParams *spatialParams, int i, int loc) {
  return getPossiblySpatial(spatialParams->parameters[i].value, loc, isSpatial(spatialParams, i));
}


/* Set parameter i, location loc to have given value
   If parameter is non-spatial, ignore loc
   If loc = -1, set this parameter at ALL LOCATIONS to have given value
   PRE: 0 <= i < spatialParams->numParameters
        if this parameter is spatial, then -1 <= loc < spatialParams->numLocs
*/
void setSpatialParam(SpatialParams *spatialParams, int i, int loc, double value) {
  setPossiblySpatial(spatialParams->parameters[i].value, loc, value, isSpatial(spatialParams, i), spatialParams->numLocs);
}


/* Return guess value of parameter i, location loc (if this parameter is non-spatial, ignore loc)
   PRE: 0 <= i < spatialParams->numParameters
        if this parameter is spatial, then 0 <= loc < spatialParams->numLocs
*/
double getSpatialParamGuess(SpatialParams *spatialParams, int i, int loc) {
  return getPossiblySpatial(spatialParams->parameters[i].guess, loc, isSpatial(spatialParams, i));
}


/* Return min. value of range of parameter i (note: this is constant across space)
   PRE: 0 <= i < spatialParams->numParameters
*/
double getSpatialParamMin(SpatialParams *spatialParams, int i) {
  return spatialParams->parameters[i].min;
}


/* Return max. value of range of parameter i (note: this is constant across space)
   PRE: 0 <= i < spatialParams->numParameters
*/
double getSpatialParamMax(SpatialParams *spatialParams, int i) {
  return spatialParams->parameters[i].max;
}


/* Return sigma value of parameter i (note: this is constant across space)
   PRE: 0 <= i < spatialParams->numParameters
*/
double getSpatialParamSigma(SpatialParams *spatialParams, int i) {
  return spatialParams->parameters[i].sigma;
}


/* Return knob value of parameter i, location loc (if this parameter is non-spatial, ignore loc)
   PRE: 0 <= i < spatialParams->numParameters
        if this parameter is spatial, then 0 <= loc < spatialParams->numLocs
*/
double getSpatialParamKnob(SpatialParams *spatialParams, int i, int loc) {
  return getPossiblySpatial(spatialParams->parameters[i].knob, loc, isSpatial(spatialParams, i));
}


/* Set "knob" of parameter i, location loc to have given value
   If parameter is non-spatial, ignore loc
   If loc = -1, set knob value of this parameter at ALL LOCATIONS to have given value
   PRE: 0 <= i < spatialParams->numParameters
        if this parameter is spatial, then -1 <= loc < spatialParams->numLocs
*/
void setSpatialParamKnob(SpatialParams *spatialParams, int i, int loc, double value) {
  setPossiblySpatial(spatialParams->parameters[i].knob, loc, value, isSpatial(spatialParams, i), spatialParams->numLocs);
}


/* Return best value of parameter i, location loc (if this parameter is non-spatial, ignore loc)
   PRE: 0 <= i < spatialParams->numParameters
        if this parameter is spatial, then 0 <= loc < spatialParams->numLocs
*/
double getSpatialParamBest(SpatialParams *spatialParams, int i, int loc) {
  return getPossiblySpatial(spatialParams->parameters[i].best, loc, isSpatial(spatialParams, i));
}


/* Set best value of parameter i, location loc to have given value
   If parameter is non-spatial, ignore loc
   If loc = -1, set best value of this parameter at ALL LOCATIONS to have given value
   PRE: 0 <= i < spatialParams->numParameters
        if this parameter is spatial, then -1 <= loc < spatialParams->numLocs
*/
void setSpatialParamBest(SpatialParams *spatialParams, int i, int loc, double value) {
  setPossiblySpatial(spatialParams->parameters[i].best, loc, value, isSpatial(spatialParams, i), spatialParams->numLocs);
}


/* Set best value of all parameters at given location to be equal to their current value
   If loc = -1, set best value of all parameters at ALL locations to be equal to their current value
*/
void setAllSpatialParamBests(SpatialParams *spatialParams, int loc) {
  int numParams;
  int firstLoc, lastLoc;
  int i, currLoc;
  double value;

  numParams = spatialParams->numParameters;
  if (loc == -1) { // set best at all locations
    firstLoc = 0;
    lastLoc = spatialParams->numLocs - 1;
  }
  else // set best at a single location
    firstLoc = lastLoc = loc;

  for (i = 0; i < numParams; i++) {
    if (isSpatial(spatialParams, i)) {
      for (currLoc = firstLoc; currLoc <= lastLoc; currLoc++) {
        value = getSpatialParam(spatialParams, i, currLoc);
	setSpatialParamBest(spatialParams, i, currLoc, value);
      }
    }
    else { // non-spatial
      value = getSpatialParam(spatialParams, i, 0);
      setSpatialParamBest(spatialParams, i, 0, value);
    }
  }
}


/* Return the index (into spatialParams->parameters) of a randomly-chosen spatial parameter that is changeable 
   PRE: random number generator has been seeded
*/
int randomChangeableSpatialParam(SpatialParams *spatialParams) {
  int rnd; 

  rnd = (int) floor(spatialParams->numChangeableParams * (random()*1.0/(RAND_MAX + 1.0))); // 0 <= rnd < spatialParams->numChangeableParams

  return spatialParams->changeableParamIndices[rnd]; 
}


/* Load spatial parameters into memory locations pointed to by paramPtrs
   For spatially-varying parameters, load value at location given by loc
   (Purpose: load all parameters into a localized structure for a model run, to make the model run more efficient)
   
   PRE: spatialParams->parameters is loaded with param. values (particularly, at given location)
	externalLoc pointers have been set for each parameter
	0 <= loc < spatialParams->numLocs
*/
void loadSpatialParams(SpatialParams *spatialParams, int loc) {
  int i, numParams;
  double value;

  numParams = spatialParams->numParameters;
  for (i = 0; i < numParams; i++) {
    // value will equal NULL if it hasn't been set yet; externalLoc will equal NULL if it was assigned to NULL in readOneSpatialParam
    if (spatialParams->parameters[i].value != NULL && spatialParams->parameters[i].externalLoc != NULL) { 
      value = getSpatialParam(spatialParams, i, loc);
      *(spatialParams->parameters[i].externalLoc) = value; // copy value into location pointed to by this externalLoc
    }
  }
}


/* If randomReset = 0, set values of all parameters to be equal to guess values
   If randomReset non-zero, set parameter values to be somewhere (chosen uniform randomly) between min and max
   (If randomReset non-zero, random number generator must have been seeded)
   Set best values of all parameters to be equal to current values
   Set knobs of all parameters to be equal to knob argument
*/
void resetSpatialParams(SpatialParams *spatialParams, double knob, int randomReset) {
  int numParams, numLocs;
  int i, loc;
  double value;

  numParams = spatialParams->numParameters;
  numLocs = spatialParams->numLocs;

  for (i = 0; i < numParams; i++) {
    if (isSpatial(spatialParams, i)) {
      for (loc = 0; loc < numLocs; loc++) {
	if (randomReset == 0) // non-random: use guess value
	  value = getSpatialParamGuess(spatialParams, i, loc);
	else // random: set parameter value to be somewhere between min and max
	  value = getSpatialParamMin(spatialParams, i) + (getSpatialParamMax(spatialParams, i) - getSpatialParamMin(spatialParams, i)) 
	    * ((float)random()/RAND_MAX);
	setSpatialParam(spatialParams, i, loc, value);
	setSpatialParamBest(spatialParams, i, loc, value);
	setSpatialParamKnob(spatialParams, i, loc, knob);
      }
    }
    else { // non-spatial
      if (randomReset == 0) // non-random: use guess value
	value = getSpatialParamGuess(spatialParams, i, 0);
      else // random: set parameter value to be somewhere between min and max
	  value = getSpatialParamMin(spatialParams, i) + (getSpatialParamMax(spatialParams, i) - getSpatialParamMin(spatialParams, i))
	    * ((float)random()/RAND_MAX);
      setSpatialParam(spatialParams, i, 0, value);
      setSpatialParamBest(spatialParams, i, 0, value);
      setSpatialParamKnob(spatialParams, i, 0, knob);
    }
  }
}


/* Write best parameter values, and other parameter info, to files
   Note that other parameter info won't have changed since the read - we just copy that over
   Write all parameter info to file with name *paramFile, and spatially-varying parameter values to file with name *spatialParamFile
   See readOneSpatialParam for file formats
*/
void writeBestSpatialParams(SpatialParams *spatialParams, char *paramFile, char *spatialParamFile) {
  FILE *paramF, *spatialParamF;
  int numParams, numLocs;
  int i, j;

  paramF = openFile(paramFile, "w");
  spatialParamF = openFile(spatialParamFile, "w");

  numParams = spatialParams->numParameters;
  numLocs = spatialParams->numLocs;

  fprintf(spatialParamF, "%d\n", numLocs);

  for (i = 0; i < numParams; i++) { // loop through parameters
    fprintf(paramF, "%s\t", spatialParams->parameters[i].name);
    if (isSpatial(spatialParams, i)) { 
      fprintf(paramF, "*\t"); // write "*" in place of value, then write values to spatialParamFile
      fprintf(spatialParamF, "%s", spatialParams->parameters[i].name);
      for (j = 0; j < numLocs; j++) // loop through locations
	fprintf(spatialParamF, "\t%f", getSpatialParamBest(spatialParams, i, j));
      fprintf(spatialParamF, "\n");
    }
    else // non-spatial: write value
      fprintf(paramF, "%f\t", getSpatialParamBest(spatialParams, i, 0));

    // now write other info to file (other info is always non-spatial):
    fprintf(paramF, "%d\t%f\t%f\t%f\n", isChangeable(spatialParams, i), getSpatialParamMin(spatialParams,i),
	    getSpatialParamMax(spatialParams, i), getSpatialParamSigma(spatialParams, i));
  }

  fclose(paramF);
  fclose(spatialParamF);
}
    

/* Write name, guess, min & max, value, best and knob of each changeable param to file
   If loc >= 0, write info for that location; if loc = -1, write value at location 0, and append a * to spatial parameter values
   PRE: outF is open for writing
*/
void writeChangeableParamInfo(SpatialParams *spatialParams, int loc, FILE *outF) {
  int i, numParams, count;
  int theLoc; // location that we're printing info for
  char appendage[2];

  if (loc == -1)
    theLoc = 0;
  else
    theLoc = loc;

  numParams = spatialParams->numParameters;
  count = 0; // count of changeable parameters
  fprintf(outF, "Changeable Parameters:\n");
  for (i = 0; i < numParams; i++) {
    if (isChangeable(spatialParams, i)) {
      if (loc == -1 && isSpatial(spatialParams, i)) // we'll append a * to spatially-varying values
	strcpy(appendage, "*");
      else
	strcpy(appendage, "");

      fprintf(outF, "[%d] %s:\t", count, spatialParams->parameters[i].name);
      fprintf(outF, "Value = %f%s\t", getSpatialParam(spatialParams, i, theLoc), appendage);
      fprintf(outF, "Best = %f%s\t", getSpatialParamBest(spatialParams, i, theLoc), appendage);
      fprintf(outF, "Knob = %f%s\t", getSpatialParamKnob(spatialParams, i, theLoc), appendage);
      fprintf(outF, "Prior: %f%s ", getSpatialParamGuess(spatialParams, i, theLoc), appendage);
      fprintf(outF, " [%f, %f]\n", getSpatialParamMin(spatialParams, i), getSpatialParamMax(spatialParams, i));

      count++;
    }
  }
}


// Clean up: deallocate spatialParams and any other dynamically-allocated pointers that need deallocating
void deleteSpatialParams(SpatialParams *spatialParams) {
  int numParameters, i;
  OneSpatialParam *param;

  // first loop through individual parameters, de-allocating space they use:
  numParameters = spatialParams->numParameters;
  for (i = 0; i < numParameters; i++) {
    param = &(spatialParams->parameters[i]);
    if (param->value != NULL) { // this parameter has been used - so space has been allocated for its various values
      free(param->value);
      free(param->guess);
      free(param->best);
      free(param->knob);
    }
  }

  // now free space used by spatialParams structure itself:
  free(spatialParams->parameters);
  free(spatialParams->changeableParamIndices);
}


  

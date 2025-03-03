// header file for spatialParams.c
// includes definition of SpatialParams structure

#ifndef SPATIAL_PARAMS_H
#define SPATIAL_PARAMS_H

// struct to hold a single (possibly) spatially-varying param
typedef struct OneSpatialParamStruct {
  char name[64]; // name of parameter
  int numLocs; // 0 if this parameter is non-spatial
  int isChangeable; // 0 or 1
  double *value; /* if non-spatial, points to a single double
		    if spatial, points to a vector of doubles
		    before it's set, value = NULL */
  double *externalLoc; // a pointer to a (non-spatial) copy of this parameter value elsewhere (e.g. local to the model)

  // items that we care about when doing parameter optimization:
  // parameter info read from file:
  double *guess; // possibly spatial; if non-changeable, ptr will be void
  double min; // left-hand side of allowable range
  double max; // right-hand side of allowable range
  double sigma; // parameter uncertainty

  // variables related to optimization:
  double *best; // best value found so far; possibly patial; if non-changeable, ptr will be void
  double *knob; /* a "knob" that can be twisted to adjust this parameter's optimization (can be used however the individual optimization scheme wants)
		   possibly spatial; if non-changeable, ptr will be void */
  
} OneSpatialParam;

typedef struct SpatialParamsStruct {
  OneSpatialParam *parameters; // vector of parameters, dynamically-allocated
  int numParameters; // number of parameters
  int numChangeableParams; // number of changeable params (in an optimization)
  int *changeableParamIndices; // indices (in parameters vector) of the changeable params (length of this vector is numChangeableParams)
  int numLocs; // number of spatial locations
} SpatialParams;


// allocate space for a new spatialParams structure, return a pointer to it
SpatialParams *newSpatialParams(int numParameters, int numLocs);


// return 1 if all parameters have been read in, 0 if not
int allParamsRead(SpatialParams *spatialParams);


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
void readOneSpatialParam(SpatialParams *spatialParams, FILE *paramFile, FILE *spatialParamFile, char *name, double *externalLoc);


/* Return 1 if parameter i varies spatially, 0 if not
   PRE: 0 <= i < spatialParams->numParameters 
*/
int isSpatial(SpatialParams *spatialParams, int i);


/* Return 1 if parameter i is changeable, 0 if not
   PRE: 0 <= i < spatialParams->numParameters 
*/
int isChangeable(SpatialParams *spatialParams, int i);


/* Check to see whether value is within allowable range of parameter i
   Return 1 if okay, 0 if bad
*/
int checkSpatialParam(SpatialParams *spatialParams, int i, double value);


/* Return value of parameter i, location loc (if this parameter is non-spatial, ignore loc) 
   PRE: 0 <= i < spatialParams->numParameters
        if this parameter is spatial, then 0 <= loc < spatialParams->numLocs
*/
double getSpatialParam(SpatialParams *spatialParams, int i, int loc);


/* Set parameter i, location loc to have given value
   If parameter is non-spatial, ignore loc
   If loc = -1, set this parameter at ALL LOCATIONS to have given value
   PRE: 0 <= i < spatialParams->numParameters
        if this parameter is spatial, then -1 <= loc < spatialParams->numLocs
*/
void setSpatialParam(SpatialParams *spatialParams, int i, int loc, double value);


/* Return guess value of parameter i, location loc (if this parameter is non-spatial, ignore loc)
   PRE: 0 <= i < spatialParams->numParameters
        if this parameter is spatial, then 0 <= loc < spatialParams->numLocs
*/
double getSpatialParamGuess(SpatialParams *spatialParams, int i, int loc);


/* Return min. value of range of parameter i (note: this is constant across space)
   PRE: 0 <= i < spatialParams->numParameters
*/
double getSpatialParamMin(SpatialParams *spatialParams, int i);


/* Return max. value of range of parameter i (note: this is constant across space)
   PRE: 0 <= i < spatialParams->numParameters
*/
double getSpatialParamMax(SpatialParams *spatialParams, int i);


/* Return sigma value of parameter i (note: this is constant across space)
   PRE: 0 <= i < spatialParams->numParameters
*/
double getSpatialParamSigma(SpatialParams *spatialParams, int i);


/* Return knob value of parameter i, location loc (if this parameter is non-spatial, ignore loc)
   PRE: 0 <= i < spatialParams->numParameters
        if this parameter is spatial, then 0 <= loc < spatialParams->numLocs
*/
double getSpatialParamKnob(SpatialParams *spatialParams, int i, int loc);


/* Set "knob" of parameter i, location loc to have given value
   If parameter is non-spatial, ignore loc
   If loc = -1, set knob value of this parameter at ALL LOCATIONS to have given value
   PRE: 0 <= i < spatialParams->numParameters
        if this parameter is spatial, then -1 <= loc < spatialParams->numLocs
*/
void setSpatialParamKnob(SpatialParams *spatialParams, int i, int loc, double value);


/* Return best value of parameter i, location loc (if this parameter is non-spatial, ignore loc)
   PRE: 0 <= i < spatialParams->numParameters
        if this parameter is spatial, then 0 <= loc < spatialParams->numLocs
*/
double getSpatialParamBest(SpatialParams *spatialParams, int i, int loc);


/* Set best value of parameter i, location loc to have given value
   If parameter is non-spatial, ignore loc
   If loc = -1, set best value of this parameter at ALL LOCATIONS to have given value
   PRE: 0 <= i < spatialParams->numParameters
        if this parameter is spatial, then -1 <= loc < spatialParams->numLocs
*/
void setSpatialParamBest(SpatialParams *spatialParams, int i, int loc, double value);


/* Set best value of all parameters at given location to be equal to their current value
   If loc = -1, set best value of all parameters at ALL locations to be equal to their current value
*/
void setAllSpatialParamBests(SpatialParams *spatialParams, int loc);


/* Return the index (into spatialParams->parameters) of a randomly-chosen spatial parameter that is changeable 
   PRE: random number generator has been seeded
*/
int randomChangeableSpatialParam(SpatialParams *spatialParams);


/* Load spatial parameters into memory locations pointed to by paramPtrs
   For spatially-varying parameters, load value at location given by loc
   (Purpose: load all parameters into a localized structure for a model run, to make the model run more efficient)
   
   PRE: spatialParams->parameters is loaded with param. values (particularly, at given location)
	externalLoc pointers have been set for each parameter
	0 <= loc < spatialParams->numLocs
*/
void loadSpatialParams(SpatialParams *spatialParams, int loc);


/* If randomReset = 0, set values of all parameters to be equal to guess values
   If randomReset non-zero, set parameter values to be somewhere (chosen uniform randomly) between min and max
   (If randomReset non-zero, random number generator must have been seeded)
   Set best values of all parameters to be equal to current values
   Set knobs of all parameters to be equal to knob argument
*/
void resetSpatialParams(SpatialParams *spatialParams, double knob, int randomReset);


/* Write best parameter values, and other parameter info, to files
   Note that other parameter info won't have changed since the read - we just copy that over
   Write all parameter info to file with name *paramFile, and spatially-varying parameter values to file with name *spatialParamFile
   See readOneSpatialParam for file formats
*/
void writeBestSpatialParams(SpatialParams *spatialParams, char *paramFile, char *spatialParamFile);


/* Write name, guess, min & max, value, best and knob of each changeable param to file
   If loc >= 0, write info for that location; if loc = -1, write value at location 0, and append a * to spatial parameter values
   PRE: outF is open for writing
*/
void writeChangeableParamInfo(SpatialParams *spatialParams, int loc, FILE *outF);


// Clean up: deallocate spatialParams and any other dynamically-allocated pointers that need deallocating
void deleteSpatialParams(SpatialParams *spatialParams);

#endif

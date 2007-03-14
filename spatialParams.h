// header file for spatialParams.c
// includes definition of SpatialParams structure

#ifndef SPATIAL_PARAMS_H
#define SPATIAL_PARAMS_H

// struct to hold a single (possibly) spatially-varying param
typedef struct OneSpatialParamStruct {
  char name[64]; // name of parameter
  int isRequired;  // 0 or 1; 1 indicates that we should terminate run if this parameter isn't specified in input file
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
  OneSpatialParam *parameters; // vector of parameters, dynamically-allocated (stored in order in which they were initialized)
  int maxParameters; // maximum number of parameters
  int numParameters; // actual number of parameters (tracks # that have been initialized with initializeOneSpatialParam)
  int numParamsRead; // tracks number of parameters that have been read in from file
  int *readIndices;  // indices (in parameters vector) of the parameters read from file, in the order in which they were read (this will allow us to output parameters in the same order later)
  int numChangeableParams; // number of changeable params (in an optimization)
  int *changeableParamIndices; // indices (in parameters vector) of the changeable params (length of this vector is numChangeableParams)
  int numLocs; // number of spatial locations
} SpatialParams;


// allocate space for a new spatialParams structure, return a pointer to it
SpatialParams *newSpatialParams(int maxParameters, int numLocs);


/* Initialize next spatial parameter:
   Set name of parameter equal to "name", 
    and set parameter's externalLoc pointer to point to the location given by "externalLoc" parameter
    Also set parameter's isRequired value: if true, then we'll terminate execution if this parameter is not read from file
   Note: acceptable to have externalLoc = NULL, but then it won't be assigned by loadSpatialParams
*/
void initializeOneSpatialParam(SpatialParams *spatialParams, char *name, double *externalLoc, int isRequired);


/* Read all parameters from file, setting all values in spatialParams structure appropriately
   
   Structure of paramFile:
      name  value  changeable  min  max  sigma
   where value is a number if parameter is non-spatial, or * if parameter is spatial; changeable is 0 or 1 (boolean)
   If parameter is spatial, look in spatialParamFile for value, which has the following structure:
      name  value  [value  value...]
   where the number of values is equal to the number of spatial locations

   The order of the parameters in paramFile does not matter,
    but parameters must be specified in the same order in paramFile and spatialParamFile

   ! is a comment character in paramFile: anything after a ! on a line is ignored 
    note, though, that comments are not currently allowed in spatialParamFile

   PRE: paramFile is open and file pointer points to start of file
        spatialParamFile is open and file pointer points to 2nd line (after the numLocs line)
 */
void readSpatialParams(SpatialParams *spatialParams, FILE *paramFile, FILE *spatialParamFile);


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
    (Note: ignores parameters that were never read in)
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
    - Note that non-changeable parameters will still be set to their guess values, though
    (Note: ignores parameters that were never read in)
   (If randomReset non-zero, random number generator must have been seeded)
   Set best values of all parameters to be equal to current values
   Set knobs of all parameters to be equal to knob argument
*/
void resetSpatialParams(SpatialParams *spatialParams, double knob, int randomReset);


/* Write best parameter values, and other parameter info, to files
    (only write parameters that were read in, and in the same order that they were read in)
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

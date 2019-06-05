/* makeSpatialInputs: A stand-alone program

   Given files with the anomalies in a bunch of locations,
   create spatial parameter and climate files,
   in order to do a spatial forward run of SIPNET

   The anomaly files should be named <varname>.anomalies,
    and should have a single line with all of the anomaly values
   The total number of values is given in the input (configuration) file

   The operation of this program is controlled by an input file,
    whose name is given by NAMELIST_FILE
   This file is read using the namelist utility
   It contains 4 variables:
    CONTROL_FILENAME (see below for format of control file)
    INPUT_FILENAME (we'll read from <INPUT_FILENAME>.param, etc.)
    OUTPUT_FILENAME (we'll print to <OUTPUT_FILENAME>.param, etc.)
    NUM_LOCS (number of spatial locations)

   Format of control file:
   Comments, denoted by '!', are ignored
   Each non-comment line should contain 4 columns: 
    Name Spatial? Col Op_type
    Name: variable name
    Spatial?: Yes | No
    Col: If a climate variable, column of this variable in .clim file
     (column 0 would be loc)
     If a parameter, col=0
    Op_type: + | * (are anomalies additive or multiplicative?)

   Author: Bill Sacks
   Creation date: 6/1/07
*/


#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "util.h"
#include "namelistInput.h"

#define NAMELIST_FILE "spatial.in"
#define NUM_CLIM_VARS 13  // number of climate variables, not counting loc
#define NUM_CLIM_INTS 2  // the first NUM_CLIM_INTS climate variables are integers
#define MAX_NAME 64
#define MAX_FILENAME 256


// ********** TYPE DEFINITIONS **********


// structure to hold information for a spatially-varying parameter or climate variable
typedef struct SpatialVarStruct  {
  char name[MAX_NAME];
  int col;  /* if a climate variable, column of this variable in .clim file (column 0 = loc)
	       if a parameter, col=0 */
  char op_type;  // + | *  (are anomalies additive or multiplicative)
  double *anomalies;
  int numLocs;  // size of anomalies array
  
  struct SpatialVarStruct *nextItem;  // for linked list
}  SpatialVar;


// structure to hold the SpatialVars that represent parameters
typedef struct SpatialParamsStruct  {
  SpatialVar *head;
  SpatialVar *tail;

  int count;  // number of items in the list, not counting the dummy item at the head
} SpatialParams;


// structure to hold the SpatialVars that represent climate variables
typedef struct SpatialClimStruct  {
  SpatialVar *elements[NUM_CLIM_VARS];  // array of pointers
} SpatialClim;


// structure to hold all of the climate variables for each step
typedef struct ClimateVarsStruct  {
  double climArray[NUM_CLIM_VARS];

  struct ClimateVarsStruct *nextClim;  // pointer to the next time step in the linked list
} ClimateVars;


// ********** SpatialVar FUNCTIONS **********


// allocate space for a new spatialVar structure, return a pointer to it
// anomalies array will be NULL for now, as will nextItem
SpatialVar *newSpatialVar(char *name, int col, char op_type)  {
  SpatialVar *spatialVar;

  if (strlen(name) >= MAX_NAME)  {
    printf("ERROR in newSpatialVar: variable name '%s' exceeds maximum length of %d\n", name, MAX_NAME);
    exit(1);
  }

  spatialVar = (SpatialVar *)malloc(sizeof(SpatialVar));

  strcpy(spatialVar->name, name);
  spatialVar->col = col;
  spatialVar->op_type = op_type;
  spatialVar->anomalies = NULL;
  spatialVar->numLocs = 0;
  spatialVar->nextItem = NULL;

  return spatialVar;
}


/* Read anomalies from file named <spatialVar->name>.anomalies
   Put these anomalies in spatialVar->anomalies
   This file must contain numLocs values, which can all be on the same line, or spread across multiple lines
 */
void readAnomalies(SpatialVar *spatialVar, int numLocs)  {
  FILE *f;
  char fileName[MAX_NAME + 10];
  int i, numRead;

  strcpy(fileName, spatialVar->name);
  strcat(fileName, ".anomalies");
  f = openFile(fileName, "r");

  spatialVar->anomalies = (double *)malloc(numLocs * sizeof(double));
  spatialVar->numLocs = numLocs;
  for (i = 0; i < numLocs; i++)  {
    numRead = fscanf(f, "%lf", &(spatialVar->anomalies[i]));
    if (numRead < 1)  {
      printf("ERROR in readAnomalies: unexpected end of input (i = %d)\n", i);
      exit(1);
    }
  }

  fclose(f);
}


// free up space used by spatialVar
void deleteSpatialVar(SpatialVar *spatialVar)  {

  if (spatialVar->anomalies != NULL)  {
    free(spatialVar->anomalies);
  }

  free(spatialVar);
}


/* Return value of this variable at the given location, 
    based on its op_type and the given baseValue
   baseValue: The value to which the anomaly refers
    (e.g. if op_type is +, then value is baseValue + anomaly)
   loc: must be >= 0 and < spatialVar->numLocs
 */
double getValueHere(SpatialVar *spatialVar, double baseValue, int loc)  {
  double value;

  if ((loc < 0) || loc >= spatialVar->numLocs)  {
    printf("ERROR in valueHere: loc %d out of range (min 0, max %d)\n", loc, spatialVar->numLocs);
    exit(1);
  }

  switch (spatialVar->op_type)  {
  case '+':
    value = baseValue + spatialVar->anomalies[loc];
    break;
  case '*':
    value = baseValue * spatialVar->anomalies[loc];
    break;
  default:
    printf("ERROR in valueHere: op_type %c for %s invalid\n", spatialVar->op_type, spatialVar->name);
    exit(1);
  }

  return value;
}


// ********** SpatialParams FUNCTIONS **********


// allocate space for a new spatialParams structure, return a pointer to it
SpatialParams *newSpatialParams()  {
  SpatialParams *spatialParams;

  spatialParams = (SpatialParams *)malloc(sizeof(SpatialParams));
  
  spatialParams->head = newSpatialVar("", 0, '+');  // we'll keep a dummy item at the head of the list
  spatialParams->tail = spatialParams->head;
  spatialParams->count = 0;

  return spatialParams;
}


/* Add a new spatialVar to the end of the list given by spatialParams
   strlen(name) must be < MAX_NAME
   op_type should be one of '+' or '*'

   anomalies array will be NULL for now

   Returns a pointer to the new spatialVar
*/
SpatialVar *addSpatialParam(SpatialParams *spatialParams, char *name, char op_type)  {
  SpatialVar *spatialVar;
  
  spatialVar = newSpatialVar(name, 0, op_type);
  spatialParams->tail->nextItem = spatialVar;
  spatialParams->tail = spatialVar;
  spatialParams->count++;

  return spatialVar;
}


/* find spatial parameter with given name in spatialParams list
   return a pointer to this parameter
   if not found, return NULL
*/
SpatialVar *getSpatialParam(SpatialParams *spatialParams, char *name)  {
  SpatialVar *spatialVar;

  spatialVar = spatialParams->head;
  while ((spatialVar != NULL) && (strcmpIgnoreCase(spatialVar->name, name) != 0)) {
    spatialVar = spatialVar->nextItem;
  }

  return spatialVar;
}


// free up space used by spatialParams
void deleteSpatialParams(SpatialParams *spatialParams)  {
  SpatialVar *curr, *temp;

  curr = spatialParams->head;
  while(curr != NULL) {
    temp = curr;
    curr = curr->nextItem;
    deleteSpatialVar(temp);
  }

  free(spatialParams);
}


// ********** SpatialClim FUNCTIONS **********


// allocate space for a new spatialClim structure, return a pointer to it
SpatialClim *newSpatialClim()  {
  SpatialClim *spatialClim;
  int i;

  spatialClim = (SpatialClim *)malloc(sizeof(SpatialClim));
  
  for (i = 0; i < NUM_CLIM_VARS; i++)  {
    spatialClim->elements[i] = newSpatialVar("", i+1, '+');
  }

  return spatialClim;
}


/* Add a new spatialVar to the appropriate position in the spatialClim list
   strlen(name) must be < MAX_NAME
   col must be > 0 and <= NUM_CLIM_VARS
   op_type should be one of '+' or '*'

   anomalies array will be NULL for now

   Returns a pointer to the new spatialVar
*/
SpatialVar *addSpatialClim(SpatialClim *spatialClim, char *name, int col, char op_type)  {
  
  if (strlen(name) >= MAX_NAME)  {
    printf("ERROR in addSpatialClim: variable name '%s' exceeds maximum length of %d\n", name, MAX_NAME);
    exit(1);
  }

  if ((col < 1) || (col > NUM_CLIM_VARS))  {
    printf("ERROR in addSpatialClim: col %d out of bounds (min = 1, max = %d)\n", col, NUM_CLIM_VARS);
    exit(1);
  }

  strcpy(spatialClim->elements[col-1]->name, name);
  spatialClim->elements[col-1]->op_type = op_type;

  return spatialClim->elements[col-1];
}


/* return a pointer to the climate variable at the given column (1-indexing: col 0 would be loc)
   if the climate variable at this column was never set, return NULL

   col must be > 0 and <= NUM_CLIM_VARS
*/
SpatialVar *getSpatialClim(SpatialClim *spatialClim, int col)  {
  SpatialVar *spatialVar;

  if ((col < 1) || (col > NUM_CLIM_VARS))  {
    printf("ERROR in getSpatialClim: col %d out of bounds (min = 1, max = %d)\n", col, NUM_CLIM_VARS);
    exit(1);
  }

  spatialVar = spatialClim->elements[col-1];
  if (strcmp(spatialVar->name, "") == 0)  {
    return NULL;
  }
  else {
    return spatialVar;
  }
}


// free up space used by spatialClim
void deleteSpatialClim(SpatialClim *spatialClim)  {
  int i;

  for (i = 0; i < NUM_CLIM_VARS; i++)  {
    deleteSpatialVar(spatialClim->elements[i]);
  }

  free(spatialClim);
}


// ********** MAIN FUNCTIONS **********


/* Read information from file given by fileName
   See comment at top of program for format

   Store information in spatialParams and spatialClim
 */
void readControlFile(char *fileName, SpatialParams *spatialParams, SpatialClim *spatialClim, int numLocs)  {
  const char *COMMENT_CHARS = "!";  // comment characters (ignore everything after this on a line)

  FILE *f;
  char line[1024];
  int isComment;
  int numRead;
  char name[MAX_NAME];
  char spatial[16];
  int col;
  char op_type[16];
  int numSpatialVars;
  SpatialVar *spatialVar;

  f = openFile(fileName, "r");

  numSpatialVars = 0;
  while(fgets(line, sizeof(line), f) != NULL)  {  // while not EOF or error
    // remove trailing comments:
    isComment = stripComment(line, COMMENT_CHARS);

    if (!isComment)  {  // if this isn't just a comment line or blank line
      numRead = sscanf(line, "%s %s %d %s", name, spatial, &col, op_type);
      // we read op_type as a string rather than as a character because %c doesn't skip over whitespace
      if (numRead < 4)  {
	printf("ERROR in readControlFile: malformed line in %s:\n%s\n", fileName, line);
	exit(1);
      }
      
      if (strcmpIgnoreCase(spatial, "yes") == 0)  {
	numSpatialVars++;
	if (col == 0)  { // parameter
	  spatialVar = addSpatialParam(spatialParams, name, op_type[0]);
	}
	else if (col > 0)  { // climate variable
	  spatialVar = addSpatialClim(spatialClim, name, col, op_type[0]);
	}
	else  {  // col < 0
	  printf("ERROR in readControlFile: read value of %d for col, for %s\n", col, name);
	  printf("Please fix %s and re-run\n", fileName);
	}

	readAnomalies(spatialVar, numLocs);
      }
      // if it's 'no' we'll just ignore it and move on
      else if (strcmpIgnoreCase(spatial, "no") != 0)  { // neither 'yes' nor 'no'
	printf("ERROR in readControlFile: read value of %s for 'spatial', expected 'yes' or 'no'\n", spatial);
	printf("Please fix %s and re-run\n", fileName);
	exit(1);
      }
    }  // if (!isComment)
  }  // while not EOF or error

  printf("Read information for %d spatially-varying variables\n", numSpatialVars);

  fclose(f);
}


/* Spatialize parameter file
   Read from <inputFilename>.param
   Write to <outputFilename>.param and <outputFilename>.param-spatial

   Assumes that none of the parameters in <inputFilename>.param are already spatial
    (we ignore any existing <inputFilename>.param-spatial)
 */
void processParameters(char *inputFilename, char *outputFilename, SpatialParams *spatialParams, int numLocs)  {
  const char *SEPARATORS = " \t"; // characters that can separate values in parameter file
  const char *COMMENT_CHARS = "!";  // comment characters (ignore everything after this on a line)

  FILE *paramFin, *paramFout, *paramSpatialFout;
  char fullFilename[MAX_FILENAME+24];
  char line[1024];
  char lineCopy[1024];
  int isComment;
  char *pName;
  char *stringValue;
  char *remainder;
  double value, valueHere;
  char *errc = "";
  SpatialVar *spatialVar;
  int loc;

  printf("Creating parameter file....\n");

  // open files for input & output:

  strcpy(fullFilename, inputFilename);
  strcat(fullFilename, ".param");
  paramFin = openFile(fullFilename, "r");
  
  strcpy(fullFilename, outputFilename);
  strcat(fullFilename, ".param");
  paramFout = openFile(fullFilename, "w");
  strcat(fullFilename, "-spatial");
  paramSpatialFout = openFile(fullFilename, "w");

  fprintf(paramSpatialFout, "%d\n", numLocs);

  // process files:
  while(fgets(line, sizeof(line), paramFin) != NULL)  {  // while not EOF or error
    strcpy(lineCopy, line);
    isComment = stripComment(lineCopy, COMMENT_CHARS);
    if (isComment)  {  // just copy over any comment lines
      fprintf(paramFout, "%s", line);
    }
    else { // not a comment line
      pName = strtok(lineCopy, SEPARATORS);
      stringValue = strtok(NULL, SEPARATORS);
      remainder = strtok(NULL, "\n");
      if (strcmp(stringValue, "*") == 0)  {
	printf("ERROR in processParameters: parameter %s is already spatial!\n", pName);
	exit(1);
      }

      value = strtod(stringValue, &errc);
      if (strlen(errc) > 0)  {  // invalid character in parameter value
	printf("ERROR in processParameters: invalid value for %s: %s\n", pName, stringValue);
	exit(1);
      }

      spatialVar = getSpatialParam(spatialParams, pName);
      if (spatialVar != NULL)  {  // this is one of the spatial parameters that we read in
	fprintf(paramFout, "%s\t*\t%s\n", pName, remainder);
	fprintf(paramSpatialFout, "%s\t", pName);
	for (loc = 0; loc < numLocs; loc++)  {
	  valueHere = getValueHere(spatialVar, value, loc);
	  fprintf(paramSpatialFout, "%f ", valueHere);
	}
	fprintf(paramSpatialFout, "\n");
      }
      else  {  // a non-spatial parameter
	fprintf(paramFout, "%s\t%s\t%s\n", pName, stringValue, remainder);
      }
    }  // else (not a comment line)
  }  // while not EOF or error

  fclose(paramFin);
  fclose(paramFout);
  fclose(paramSpatialFout);
}
      

/* Spatialize climate file
   Read from <inputFilename>.clim
   Write to <outputFilename>.clim

   Assumes that the climate file only has one location to begin with
 */
void processClimate(char *inputFilename, char *outputFilename, SpatialClim *spatialClim, int numLocs)  {
  const char *SEPARATORS = " \t\n"; // characters that can separate values in parameter file

  FILE *fIn, *fOut;
  char fullFilename[MAX_FILENAME+24];
  char line[1024];
  char *stringValue;
  int loc, i;
  double value, valueHere;
  char *errc = "";
  SpatialVar *spatialVar;
  ClimateVars *head, *curr, *next;  // note that we'll store a single empty element at the head

  printf("Creating climate file....\n");

  strcpy(fullFilename, inputFilename);
  strcat(fullFilename, ".clim");
  fIn = openFile(fullFilename, "r");

  strcpy(fullFilename, outputFilename);
  strcat(fullFilename, ".clim");
  fOut = openFile(fullFilename, "w");

  // Read the climate file, storing all values in a linked list:

  head = (ClimateVars *)malloc(sizeof(ClimateVars));
  // note that we'll store a single empty element at the head
  curr = head;
  while(fgets(line, sizeof(line), fIn) != NULL)  {  // while not EOF or error
    next = (ClimateVars *)malloc(sizeof(ClimateVars));
    
    // first get the location, and ensure that it's location 0:
    stringValue = strtok(line, SEPARATORS);
    loc = strtol(stringValue, &errc, 0);
    if (strlen(errc) > 0)  {  // invalid character(s) in input string
      printf("ERROR in processClimate: Invalid value for loc: %s\n", stringValue);
      exit(1);
    }
    if (loc != 0)  {
      printf("ERROR in processClimate: Read loc = %d, but input climate file must contain only location 0\n", loc);
      exit(1);
    }

    // now put the rest of the values in the climate array:
    for(i = 0; i < NUM_CLIM_VARS; i++)  {
      stringValue = strtok(NULL, SEPARATORS);
      if (stringValue == NULL)  {
	printf("ERROR in processClimate: Prematurely ran out of variables from input climate file\n");
	exit(1);
      }

      value = strtod(stringValue, &errc);
      if (strlen(errc) > 0)  {  // invalid character(s) in input string
	printf("ERROR in processClimate: Invalid value for climate variable: %s\n", stringValue);
	exit(1);
      }

      next->climArray[i] = value;
    }  // for i

    curr->nextClim = next;
    curr = next;
  }  // while not EOF or error

  // Now we process the climate variables, and put spatially-varying values in output file:

  for(loc = 0; loc < numLocs; loc++)  {
    if (loc % 10 == 0) {
      printf("Processing location %d....\n", loc);
    }

    curr = head->nextClim;  // head is a dummy place-holder, so we start with next
    while (curr != NULL)  {   // loop through time steps
      fprintf(fOut, "%d\t", loc);
      for (i = 0; i < NUM_CLIM_VARS; i++)  {
	value = curr->climArray[i];
	spatialVar = getSpatialClim(spatialClim, i+1);
	if (spatialVar == NULL)  {  // no spatial information read for this climate variable
	  valueHere = value;
	}
	else  {  // spatial information read: apply anomaly
	  valueHere = getValueHere(spatialVar, value, loc);
	}

	if (i < NUM_CLIM_INTS)  {  // this is actually an integer variable, not a double
	  fprintf(fOut, "%.0f\t", valueHere);  // so print it as an integer, with no decimal point
	}
	else  {
	  fprintf(fOut, "%f\t", valueHere);
	}
      }  // for i

      fprintf(fOut, "\n");
      curr = curr->nextClim;
    }  // while (loop through time steps)
  }  // for loc

  // Finally, it's cleanup time:
      
  fclose(fIn);
  fclose(fOut);

  // de-allocate space for climate linked list:
  curr = head;
  while(curr != NULL)  {
    next = curr->nextClim;
    free(curr);
    curr = next;
  }
}


int main(int argc, char *argv[])  {
  NamelistInputs *namelistInputs;
  char controlFilename[MAX_FILENAME];
  char inputFilename[MAX_FILENAME];
  char outputFilename[MAX_FILENAME];
  int numLocs;
  SpatialParams *spatialParams;
  SpatialClim *spatialClim;

  // setup namelist input:
  namelistInputs = newNamelistInputs();
  addNamelistInputItem(namelistInputs, "CONTROL_FILENAME", STRING_TYPE, controlFilename, MAX_FILENAME);
  addNamelistInputItem(namelistInputs, "INPUT_FILENAME", STRING_TYPE, inputFilename, MAX_FILENAME);
  addNamelistInputItem(namelistInputs, "OUTPUT_FILENAME", STRING_TYPE, outputFilename, MAX_FILENAME);
  addNamelistInputItem(namelistInputs, "NUM_LOCS", INT_TYPE, &numLocs, 0);

  // read from namelist file:
  readNamelistInputs(namelistInputs, NAMELIST_FILE);

  // and make sure we read everything:
  dieIfNotRead(namelistInputs, "CONTROL_FILENAME");
  dieIfNotRead(namelistInputs, "INPUT_FILENAME");
  dieIfNotRead(namelistInputs, "OUTPUT_FILENAME");
  dieIfNotRead(namelistInputs, "NUM_LOCS");

  if (numLocs < 1)  {
    printf("ERROR: invalid numLocs (%d): must be >= 1\n", numLocs);
    printf("Please fix %s and re-run\n", NAMELIST_FILE);
    exit(1);
  }

  spatialParams = newSpatialParams();
  spatialClim = newSpatialClim();

  readControlFile(controlFilename, spatialParams, spatialClim, numLocs);
  processParameters(inputFilename, outputFilename, spatialParams, numLocs);
  processClimate(inputFilename, outputFilename, spatialClim, numLocs);

  deleteSpatialParams(spatialParams);
  deleteSpatialClim(spatialClim);

  return 0;
}

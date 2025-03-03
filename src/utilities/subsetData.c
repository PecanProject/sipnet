/* subsetData: A stand-alone program

   Given a set of SIPNET input files, 
   along with a start year, start day, and number of days,
   create subsetted files (.clim, .dat, .valid, .sigma, .spd)
   that start at the given date and extend for the given # of days.

   To start at the beginning of the record, specify START_YEAR = START_DAY = -1
   To go until the end of the record, specify NUM_DAYS = -1

   Also create a file for aggregation (*.agg), using the "AGGREGATION" input
   - Right now, set up to do no aggregation ("none") or daily aggregation ("daily")

   Note that you could use this program to just create desired .spd and .agg files
   without doing any subsetting (START_YEAR = START_DAY = NUM_DAYS = -1)

   Note that this only works for input files with a single location (loc=0)
   
   Author: Bill Sacks
   Creation date: 6/6/07
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "common/namelistInput.h"
#include "common/util.h"

#define NAMELIST_FILE "subset.in"
#define MAX_LINE 1024  // maximum line length for input files, including terminating '\0'
#define MAX_FILENAME 256  // maximum length of input and output base file names
#define MAX_NAME 64  // maxiumum length of other miscellaneous strings

// named constants for the different time scales of aggregation
enum AGGREGATION_TYPES { NO_AGGREGATION, DAILY_AGGREGATION };


// If status==NULL, print message and exit; otherwise do nothing
void checkStatus(char *status, char *message)  {
  if (status == NULL)  {
    printf("%s", message);
    exit(1);
  }
}


/* Read next line from climate file f, which must be open for reading
   Return the line in *line (up to maxLength characters)
   Also return the location, year and day in their respective variables

   Return value: a pointer to line if read was successful, otherwise NULL
*/
char *readClimLine(FILE *f, char *line, int maxLength, int *loc, int *year, int *day)  {
  char *status; // return value

  status = fgets(line, maxLength, f);
  if (status != NULL)
    sscanf(line, "%d %d %d", loc, year, day);
    
  return status;
}


/* Make sure climFin only contains data for a single location
   If not, die with an error message

   climFin should be open for reading and rewound; it is rewound before exiting this function
 */
void checkLocations(FILE *climFin)  {
  char line[MAX_LINE];
  int loc, year, day;
  char *status;

  while ((status = readClimLine(climFin, line, MAX_LINE, &loc, &year, &day)) != NULL)  {  // while not EOF or error
    if (loc != 0)  {
      printf("ERROR: Read loc = %d, but input climate file must contain only location 0\n", loc);
      exit(1);
    }
  }

  rewind(climFin);
}



/* return 1 if (year, day) is on or after (startYear, startDay),
   return 0 otherwise
*/
int pastStart(int year, int day, int startYear, int startDay)  {
  return ((year > startYear) || ((year == startYear) && (day >= startDay)));
}


/* Read lines from clim, data, valid and sigma files,
   until we reach a point on or after the date given by startYear, startDay
   (startDay is Julian day of year)

   startYear and startDay may be modified to hold the ACTUAL starting year and day
    (which may be different from the values passed in - for example if these values are -1,
    indicating that we should just start at the beginning)

   The three files should be open for reading and rewound

   At the completion of this function, the file pointers for the three files
   will point to the start of the first line on or after the given date

   This is only implemented for files containing a single location
*/
void gotoStart(FILE *climFin, FILE *dataFin, FILE *validFin, FILE *sigmaFin, int *startYear, int *startDay)  {
  char line[MAX_LINE];
  fpos_t fpos;  // file position in climFin, so we can "unread" the last line
  char *status;
  int loc, year, day;
  int err;

  // Read first line:
  err = fgetpos(climFin, &fpos);
  if (err)  {
    printf("Error in fgetpos\n");
    exit(1);
  }
  status = readClimLine(climFin, line, MAX_LINE, &loc, &year, &day);
  checkStatus(status, "ERROR reading climate file: no valid data\n");

  // Keep reading lines until we find the start day:
  while (!pastStart(year, day, *startYear, *startDay))  {
    status = fgets(line, MAX_LINE, dataFin); // read & ignore line from data file
    checkStatus(status, "ERROR reading data file: Unexpected EOF or error\n");
    status = fgets(line, MAX_LINE, validFin); // read & ignore line from valid file
    checkStatus(status, "ERROR reading valid file: Unexpected EOF or error\n");
    status = fgets(line, MAX_LINE, sigmaFin); // read & ignore line from valid file
    checkStatus(status, "ERROR reading sigma file: Unexpected EOF or error\n");
    
    // read next line from climate file:
    fgetpos(climFin, &fpos);
    if (err)  {
      printf("Error in fgetpos\n");
      exit(1);
    }
    status = readClimLine(climFin, line, MAX_LINE, &loc, &year, &day);
    checkStatus(status, "ERROR reading climate file: reached EOF before finding desired start date\n");
  }  // while !pastStart

  // set actual starting year and day:
  *startYear = year;
  *startDay = day;

  // now we've read one extra line from climate file: have to unread it:
  err = fsetpos(climFin, &fpos);
  if (err)  {
    printf("Error in fsetpos\n");
    exit(1);
  }
}


/* Starting at current positions in climate, data, and valid input files,
   copy from input files to output files until we've read numDays days (numDays <= 0 indicates that we should read until end of file).
   Also create spdFout (steps per day; including leading startYear, startDay) and aggFout (# steps per aggregation time step)
    - Aggregation time step determined by aggType (must be one of the AGGREGATION_TYPES)
    - Right now, only implemented for NO_AGGREGATION (1 step per aggregation step) and DAILY_AGGREGATION

   All output files should be open for writing

   Assumes that there is at least one time step per day in the input files
    (i.e. the interval between two time steps is not greater than a day)
 */
void copySubset(FILE *climFin, FILE *dataFin, FILE *validFin, FILE *sigmaFin, FILE *climFout, FILE *dataFout, FILE *validFout, FILE *sigmaFout, FILE *spdFout, FILE *aggFout, int aggType, int startYear, int startDay, int numDays)  {
  char line[MAX_LINE];
  int loc, year, day;
  int currYear, currDay;
  int ndays;  // number of days we've read so far (including partial days)
  int stepsThisDay;
  int done;
  char *status;
  
  fprintf(spdFout, "%d %d ", startYear, startDay);
  
  // read next line from climate file:
  status = readClimLine(climFin, line, MAX_LINE, &loc, &year, &day);
  checkStatus(status, "ERROR reading climate file: no data found after start date\n");

  ndays = 1;
  stepsThisDay = 1;
  done = 0;
  while (!done)  {
    currYear = year;
    currDay = day;

    fputs(line, climFout);

    // read & copy line from data file
    status = fgets(line, MAX_LINE, dataFin); 
    checkStatus(status, "ERROR reading data file: Unexpected EOF or error\n");
    fputs(line, dataFout);

    // read & copy line from valid file
    status = fgets(line, MAX_LINE, validFin); 
    checkStatus(status, "ERROR reading valid file: Unexpected EOF or error\n");
    fputs(line, validFout);

    // read & copy line from sigma file
    status = fgets(line, MAX_LINE, sigmaFin); 
    checkStatus(status, "ERROR reading sigma file: Unexpected EOF or error\n");
    fputs(line, sigmaFout);
    
    // read next line from climate file:
    // note that we copy it to output file at TOP of loop
    status = readClimLine(climFin, line, MAX_LINE, &loc, &year, &day);

    if (status == NULL)  {  // EOF or error
      // do final aggregation and spd file output:
      switch (aggType)  {
      case NO_AGGREGATION:
	fprintf(aggFout, "1 -1\n");
	break;
      case DAILY_AGGREGATION:
	fprintf(aggFout, "%d -1\n", stepsThisDay);
	break;
      default:
	printf("Invalid aggregation type: %d\n", aggType);
	exit(1);
      }
      fprintf(spdFout, "%d -1\n", stepsThisDay);

      if (numDays <= 0) // reading until end of file... which we have now reached
	done = 1;
      else if (ndays == numDays)  // we read all the days we expected
	done = 1;
      else  {  // we haven't read enough days
	printf("ERROR reading climate file: reached EOF or error after reading only %d of %d days\n", ndays, numDays);
	printf("Try choosing an earlier start date, or a smaller number of days\n");
	exit(1);
      }
    }

    else  {  // not EOF or error
      if ((year - currYear > 1) || ((year - currYear == 1) && (currDay < 365 || day > 1)) || ((year == currYear) && (day - currDay) > 1))  {
	// if we (1) advanced by more than 1 year, or (2) advanced by one year, but we either (a) weren't on day 365 (or 366), or (b) now aren't on day 1, or (3) advanced by more than one day
	// (note: we don't explicitly check for climate going backwards: we assume this doesn't happen)
	printf("ERROR: subsetData only implemented for input files with at least one step per day\n");
	exit(1);
      }
      
      // possibly do aggregation file output:
      switch (aggType)  {
      case NO_AGGREGATION:
	fprintf(aggFout, "1 ");
	break;
      case DAILY_AGGREGATION:
	if (day != currDay)
	  fprintf(aggFout, "%d ", stepsThisDay);
	break;
      default:
	printf("Invalid aggregation type: %d\n", aggType);
	exit(1);
      }
	
      if (day != currDay)  {
	fprintf(spdFout, "%d ", stepsThisDay);
	ndays++;
	stepsThisDay = 1;
	// we set new currDay and currYear at TOP of loop

	if ((numDays > 0) && (ndays > numDays))  {  // we've read numDays days
	  done = 1;

	  /* output end-of-line terminators to aggregation and spd files
	     Note that, because the only possible aggregations are NO_AGGREGATION and DAILY_AGGREGATION, 
	     we will definitely have already output the last agg. step to aggregation file.
	     If we add another aggregation type (e.g. annual), we may need to output the last value here before terminating the line
	  */
	  fprintf(aggFout, "-1\n");
	  fprintf(spdFout, "-1\n");
	}
      }
      else  { // day == currDay
	stepsThisDay++;
      }
    }  // else (not EOF or error)
  }  // while !done
}  // copySubset


// copy contents of <inFilename> to <outFilename>
void copyFile(char *inFilename, char *outFilename)  {
  FILE *in, *out;
  char c;

  in = openFile(inFilename, "r");
  out = openFile(outFilename, "w");

  while ((c = fgetc(in)) != EOF)
    fputc(c, out);
    
  fclose(out);
  fclose(in);
}


/* Copy contents of <inFilename>.param and .param-spatial  
   to <outFilename>.param and .param-spatial
 */
void copyOtherFiles(char *inFilename, char *outFilename)  {
  char *inFullname, *outFullname;

  inFullname = (char *)malloc((strlen(inFilename) + 25) * sizeof(char));
  outFullname = (char *)malloc((strlen(outFilename) + 25) * sizeof(char));

  // copy .param file
  strcpy(inFullname, inFilename);
  strcat(inFullname, ".param");
  strcpy(outFullname, outFilename);
  strcat(outFullname, ".param");
  copyFile(inFullname, outFullname);

  // copy .param-spatial file
  strcpy(inFullname, inFilename);
  strcat(inFullname, ".param-spatial");
  strcpy(outFullname, outFilename);
  strcat(outFullname, ".param-spatial");
  copyFile(inFullname, outFullname);


  free(inFullname);
  free(outFullname);
}
  


int main(int argc, char *argv[])  {
  NamelistInputs *namelistInputs;
  char inFilename[MAX_FILENAME];  // base input file name
  char outFilename[MAX_FILENAME];  // base output file name
  int startYear, startDay, numDays;
  char aggTypeString[MAX_NAME];
  int aggType;  // one of AGGREGATION_TYPES
  FILE *climFin, *dataFin, *validFin, *sigmaFin, *climFout, *dataFout, *validFout, *sigmaFout, *spdFout, *aggFout;

  // setup namelist input:
  namelistInputs = newNamelistInputs();
  addNamelistInputItem(namelistInputs, "INPUT_FILENAME", STRING_TYPE, inFilename, MAX_FILENAME);
  addNamelistInputItem(namelistInputs, "OUTPUT_FILENAME", STRING_TYPE, outFilename, MAX_FILENAME);
  addNamelistInputItem(namelistInputs, "START_YEAR", INT_TYPE, &startYear, 0);
  addNamelistInputItem(namelistInputs, "START_DAY", INT_TYPE, &startDay, 0);
  addNamelistInputItem(namelistInputs, "NUM_DAYS", INT_TYPE, &numDays, 0);
  addNamelistInputItem(namelistInputs, "AGGREGATION", STRING_TYPE, &aggTypeString, MAX_NAME);

  // read from namelist file:
  readNamelistInputs(namelistInputs, NAMELIST_FILE);

  // and make sure we read everything:
  dieIfNotRead(namelistInputs, "INPUT_FILENAME");
  dieIfNotRead(namelistInputs, "OUTPUT_FILENAME");
  dieIfNotRead(namelistInputs, "START_YEAR");
  dieIfNotRead(namelistInputs, "START_DAY");
  dieIfNotRead(namelistInputs, "NUM_DAYS");
  dieIfNotRead(namelistInputs, "AGGREGATION");

  if (strcmp(aggTypeString, "") == 0)
    aggType = NO_AGGREGATION;
  else if (strcmpIgnoreCase(aggTypeString, "daily") == 0)
    aggType = DAILY_AGGREGATION;
  else  {
    printf("Unrecognized aggregation: '%s'\n", aggTypeString);
    exit(1);
  }

  // open files:
  // input files:
  climFin = openFileExt(inFilename, "clim", "r");
  dataFin = openFileExt(inFilename, "dat", "r");
  validFin = openFileExt(inFilename, "valid", "r");
  sigmaFin = openFileExt(inFilename, "sigma", "r");

  // output files:
  climFout = openFileExt(outFilename, "clim", "w");
  dataFout = openFileExt(outFilename, "dat", "w");
  validFout = openFileExt(outFilename, "valid", "w");
  sigmaFout = openFileExt(outFilename, "sigma", "w");
  spdFout = openFileExt(outFilename, "spd", "w");
  aggFout = openFileExt(outFilename, "agg", "w");

  // do the work:
  checkLocations(climFin);  // make sure the climate file only contains one location
  gotoStart(climFin, dataFin, validFin, sigmaFin, &startYear, &startDay);
  copySubset(climFin, dataFin, validFin, sigmaFin, climFout, dataFout, validFout, sigmaFout, spdFout, aggFout, aggType, startYear, startDay, numDays);
  copyOtherFiles(inFilename, outFilename);

  // close files:
  fclose(climFin);
  fclose(dataFin);
  fclose(validFin);
  fclose(sigmaFin);
  fclose(climFout);
  fclose(dataFout);
  fclose(validFout);
  fclose(sigmaFout);
  fclose(spdFout);
  fclose(aggFout);

  return 0;
}
  

// Bill Sacks
// 7/9/02

// functions shared by different parameter estimation methods

#include <time.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "paramchange.h"

// the following variables are made global because they are computed once
// at the beginning of the program, and then must stick around (unchanging) for the whole program

static double *data; /* read in once at start of program
			 compared with model data in difference function */
static double *sigmas; // data uncertainty
static int numData;
static int startOpt, endOpt; // starting and ending indices for optimization (1-indexing)
static int *indices; /* array of indices of the changeable parameters
			i.e. indices in the param. file (0 = first param. in file, etc.) */
static int *valid; /* valid[i] indicates whether data[i] is valid (0 = invalid, non-0 = valid) 
		      (based on fraction of valid data points) */

struct AggregateInfo { // set at beginning of program
  int startPt, endPt; // indices of data point to start and stop at (1-indexing)	   
  int *spd; // steps per day
  int numDays;
  int startYear; // 4 digit year
  int startDay; // Julian day of year (1 = Jan. 1)
};

static struct AggregateInfo aggInfo;


// our own openFile method, which exits gracefully if there's an error
FILE *openFile(char *name, char *mode) {
  FILE *f;

  if ((f = fopen(name, mode)) == NULL) {
    printf("Error opening %s for %s\n", name, mode);
    exit(1);
  }

  return f;
}


// if seed = 0, seed based on time, otherwise seed based on seed
void seedRand(unsigned int seed, FILE *outF) {
  time_t t;

  if (seed == 0) {
    t = time(NULL);
    fprintf(outF, "Seeding random number generator with time: %d\n\n", (unsigned int)t);
    srand(t);
  }
  else {
    fprintf(outF, "Seeding random number generator with %d\n\n", seed);
    srand(seed);
  }
}


// returns an exponentially-distributed negative random number 
double randm() {
  double val;

  do {
    val = rand()/(RAND_MAX + 1.0);
  } while (val <= 0.0); // val should be <= 0.0 once in a blue moon
  return log(val);
}

// allocate space for an array of doubles of given size,
// return pointer to start of array
double *makeArray(int size) {
  double *ptr;
  ptr = (double *)malloc(size * sizeof(double));
  return ptr;
}

// Check to make sure the new value of parameter #(index) is within allowable bounds
// return non-zero if okay, 0 if bad  
int check(double newVal, int index, PInfo pInfo[]) {
  return (newVal >= pInfo[index].min && newVal <= pInfo[index].max);
}


/* Run modelF with given parameters, compare output with measured data
   Return a measure of the difference between measured and predicted data
   (Higher = worse) 
   Only use "valid" data points (as determined by validFrac in readData)
   And only use points between startOpt and endOpt (set in readData)
   Also takes into account error in parameter values from expected values (priors)
   paramWeight gives relative weight of parameter error to data error
   (paramWeight should probably be >= 1, or 0 to not care about parameter errors)
   [IGNORE *sigma - just return 0 - used in difference 2 - just here to have consistent headers]
*/
double difference(double *sigma, int numParams, double params[], PInfo pInfo[], double paramWeight,
		  void (*modelF)(double *, int, double *, int *)) 
{
  int i;
  double *model;
  double sumSquares;
  double thisSigma;

  model = makeArray(numData);

  (*modelF)(model, numParams, params, indices); // run model, put results in model array
  
  sumSquares = 0.0;

  if (paramWeight > 0.0) {
    // first compute parameter error, based on prior estimates:
    for (i = 0; i < numParams; i++) {
      thisSigma = pInfo[i].sigma;
      sumSquares += pow((params[i] - pInfo[i].guess), 2)/(2*thisSigma*thisSigma);
    }
    sumSquares *= paramWeight;
  }

  // now add data error:
  for (i = startOpt - 1; i < endOpt; i++) {
    if (valid[i]) {
      thisSigma = sigmas[i];
      sumSquares += pow((model[i] - data[i]), 2)/(2*thisSigma*thisSigma);
    }
  }

  free(model);

  // used to do:
  // return sumSquares/(numData + numParams*paramWeight); // this should usually be of order 1

  *sigma = 0.0; // UNUSED - JUST TO REMAIN CONSISTENT WITH OTHER DIFFERENCE FUNCTION

  return sumSquares;
}



/* Difference, version 2 - ESTIMATES SIGMA
   Run modelF with given parameters, compare output with measured data
   Return a measure of the difference between measured and predicted data
   (Higher = worse)
   Return best sigma value in *sigma
   Only use "valid" data points (as determined by validFrac in readData)
   And only use points between startOpt and endOpt (set in readData)
   [IGNORE pInfo and paramWeight - just there to be consistent with difference function above]
*/
double difference2(double *sigma, int numParams, double params[], PInfo pInfo[], double paramWeight,
		  void (*modelF)(double *, int, double *, int *)) 
{
  int i;
  double *model;
  double sumSquares;
  int n; // number of data points used in sumSquares
  double logLike; // the log likelihood

  model = makeArray(numData);

  (*modelF)(model, numParams, params, indices); // run model, put results in model array
  
  sumSquares = 0.0;
  n = 0;

  for (i = startOpt - 1; i < endOpt; i++) {
    if (valid[i]) {
      sumSquares += pow((model[i] - data[i]), 2);
	  n++;
    }
  }

  free(model);

  *sigma = sqrt(sumSquares/(double)n);
  logLike = n * log(*sigma);
  logLike += sumSquares/(2.0*(*sigma)*(*sigma));

  return logLike;
}


/* Run modelF with given parameters, compare output with measured data
   Write mean error (per day) to given file
   
   Also aggregate data over various time scales, and compare aggregated model output with data
   Write these mean errors (per day) to file as well

   Also compare interannual variabilities of model and data 
   and write these mean errors (per day) to file
*/
void aggregates(int numParams, double params[], 
		void (*modelF)(double *, int, double *, int *), FILE *aggFile) 
{
  const int LAST_DAYS[] = {31,59,90,120,151,181,212,243,273,304,334,365}; 
  // last day of each month
  const int LAST_DAYS_LY[] = {31,60,91,121,152,182,213,244,274,305,335,366};
  const int DAYS_IN_YR[] = {365,366}; /* given leapYr = 0 or 1, DAYS_IN_YR[leapYr]
				       represents the number of days in this year */
  int i, j, day, julianDay, month, year, numDays, numMonths;
  int leapYr; // 1 if this is a leap year
  const int *theseMonths; // points to either LAST_DAYS or LAST_DAYS_LY
  int index, count; /* note: right now count isn't used
		       we used to print mean error per point, which needed count */
  double *model;
  double *modelD, *dataD; // daily aggregations
  double *modelY, *dataY; // yearly aggregations
  double modelYAvg, dataYAvg, modelYVar, dataYVar;
  int maxYrs; // the maximum number of years we could have
  int yearIndex; // starting at 0 rather than being a 4-digit year
  double sum;
  double netNeeM, netNeeD; // net model and data nee for this aggregated step
  double totNetNeeM, totNetNeeD;
 
  model = makeArray(numData);
  modelD = makeArray(aggInfo.numDays);
  dataD = makeArray(aggInfo.numDays);
  maxYrs = aggInfo.numDays/365 + 2;
  modelY = makeArray(maxYrs);
  dataY = makeArray(maxYrs);

  (*modelF)(model, numParams, params, indices); // run model, put results in model array
  
  sum = 0.0;
  for (i = aggInfo.startPt - 1; i < aggInfo.endPt; i++) {
    sum += fabs(model[i] - data[i]);
  }
  fprintf(aggFile, "0.5\t%f\n", sum/aggInfo.numDays); // print mean error per day

  // fill daily aggregation arrays:
  index = aggInfo.startPt - 1;
  for (day = 0; day < aggInfo.numDays; day++) {
    netNeeM = netNeeD = 0.0;
    for (j = 0; j < aggInfo.spd[day]; j++) {
      netNeeM += model[index];
      netNeeD += data[index];
      index++;
    }
    modelD[day] = netNeeM;
    dataD[day] = netNeeD;
  }

  // first we aggregate days, then 2-days, 3-days ... 7-days:
  for (numDays = 1; numDays <= 7; numDays++) {
    day = 0;
    sum = 0.0;
    count = 0;
    while (day < aggInfo.numDays) {
      netNeeM = netNeeD = 0.0;
      for (i = 0; i < numDays && day < aggInfo.numDays; i++, day++) {
	netNeeM += modelD[day];
	netNeeD += dataD[day];
      }
      sum += fabs(netNeeM - netNeeD);
      count++;
    }
    fprintf(aggFile, "%d\t%f\n", numDays, sum/aggInfo.numDays);
  }

  // now do monthly and 3-monthly (roughly seasonal) aggregation:
  for (numMonths = 1; numMonths <= 3; numMonths += 2) {
    day = 0;
    sum = 0.0;
    count = 0;
    julianDay = aggInfo.startDay;
    year = aggInfo.startYear;
    leapYr = (year % 4 == 0);
    if (leapYr)
      theseMonths = LAST_DAYS_LY;
    else
      theseMonths = LAST_DAYS;
    // find starting month (or starting set of months):
    month = numMonths - 1; 
    while (theseMonths[month] < julianDay)
      month += numMonths;

    // now do the aggregation:
    while (day < aggInfo.numDays) {
      netNeeM = netNeeD = 0.0;
      while (julianDay <= theseMonths[month] && day < aggInfo.numDays) { 
	// loop through current month/set of months
	netNeeM += modelD[day];
	netNeeD += dataD[day];
	day++;
	julianDay++;
      }
      sum += fabs(netNeeM - netNeeD);
      count++;
      month += numMonths;
      if (month >= 12) { // HAPPY NEW YEAR!
	month = numMonths - 1;
	year++;
	leapYr = (year % 4 == 0);
	if (leapYr)
	  theseMonths = LAST_DAYS_LY;
	else
	  theseMonths = LAST_DAYS;
	julianDay = 1;
      }
    }
    fprintf(aggFile, "%d\t%f\n", 30*numMonths, sum/aggInfo.numDays);
  }

  // now do yearly and overall aggregation:

  // first fill year arrays and find yearly aggregation:
  day = 0;
  julianDay = aggInfo.startDay;
  year = aggInfo.startYear;
  leapYr = (year % 4 == 0);
  yearIndex = 0;
  totNetNeeM = totNetNeeD = 0.0;
  sum = 0.0;
  count = 0;
  while (day < aggInfo.numDays) {
    netNeeM = netNeeD = 0.0;
    while (julianDay <= DAYS_IN_YR[leapYr] && day < aggInfo.numDays) {
      // loop through current year
      netNeeM += modelD[day];
      netNeeD += dataD[day];
      day++;
      julianDay++;
    }
    // HAPPY NEW YEAR!
    sum += fabs(netNeeM - netNeeD); // for yearly aggregation
    count++;
    modelY[yearIndex] = netNeeM; // used to find interannual variability
    dataY[yearIndex] = netNeeD;
    totNetNeeM += netNeeM; // for overall aggregation
    totNetNeeD += netNeeD;
    julianDay = 1;
    year++;
    yearIndex++;
    leapYr = (year % 4 == 0);
  }
  maxYrs = yearIndex; // now maxYrs represents the actual number of years of data

  fprintf(aggFile, "365\t%f\n", sum/aggInfo.numDays);
  fprintf(aggFile, "%d\t%f\n", aggInfo.numDays, fabs(totNetNeeM - totNetNeeD)/aggInfo.numDays);

  // finally, do interannual variability:
  // first find averages over all years:
  modelYAvg = dataYAvg = 0.0;
  for (yearIndex = 0; yearIndex < maxYrs; yearIndex++) {
    modelYAvg += modelY[yearIndex];
    dataYAvg += dataY[yearIndex];
  }
  modelYAvg /= (double)maxYrs;
  dataYAvg /= (double)maxYrs;

  // now find variability of each year from avg, and find differences in variabilities
  // between model and data:
  sum = 0.0;
  count = 0;
  for (yearIndex = 0; yearIndex < maxYrs; yearIndex++) {
    modelYVar = modelY[yearIndex] - modelYAvg;
    dataYVar = dataY[yearIndex] - dataYAvg;
    sum += fabs(modelYVar - dataYVar);
    count++;
  }
  fprintf(aggFile, "0\t%f\n", sum/aggInfo.numDays);

  fprintf(aggFile, "\n\n"); // 2 blank lines indicates next set of data

  free(model);
  free(modelD);
  free(dataD);
  free(modelY);
  free(dataY);
}


/* Read measured data and sigmas into arrays
   Also read spd file (steps per day), in following format:
   first line: first_year first_julian_day missing_points_in_first_year
   (where a negative missing points in first year indicates extra points in first year)
   subsequent lines contain steps per day for each day.
   Also read .valid file and set values in valid array (based on validFrac)
   Set global numData appropriately
   Data comes from fileName.dat, Sigmas come from fileName.sigma
   
   startO and endO are indices of start and end points for optimization
   startCompare and endCompare are indices of start and end points for final comparison
   (1-indexing; end = -1 -> use numData for end point)
*/
void readData(char *fileName, double validFrac, int startO, int endO,
	      int startCompare, int endCompare, FILE *outFile) 
{
  char dataFile[64], sigmaFile[64], spdFile[64], validFile[64];
  FILE *in1, *in2;
  int status1, status2;
  int index;
  int julianDay;
  int year, leapYr;
  int spd, dataCount;
  double thisValidFrac;

  strcpy(dataFile, fileName);
  strcpy(sigmaFile, fileName);
  strcpy(spdFile, fileName);
  strcpy(validFile, fileName);
  strcat(dataFile, ".dat");
  strcat(sigmaFile, ".sigma");
  strcat(spdFile, ".spd");
  strcat(validFile, ".valid");
  
  in1 = openFile(dataFile, "r");
  in2 = openFile(sigmaFile, "r");

  // first determine number of data:
  numData = 0;
  while ((status1 = fscanf(in1, "%*f")) != EOF) // read and ignore
    numData++; // another piece of data

  data = makeArray(numData);
  sigmas = makeArray(numData);
  valid = (int *)malloc(numData * sizeof(int));

  rewind(in1); // go back to beginning
  for (index = 0; index < numData; index++) {
    status1 = fscanf(in1, "%lf", &(data[index]));
    status2 = fscanf(in2, "%lf", &(sigmas[index]));
  }

  fclose(in1);
  fclose(in2);

  startOpt = startO;
  if (endO == -1 || endO > numData)
    endOpt = numData;
  else
    endOpt = endO;

  // now find start day, start year, and steps per day

  if (endCompare == -1 || endCompare > numData) // use data up to end
    endCompare = numData;
  aggInfo.startPt = startCompare;
  aggInfo.endPt = endCompare;

  in1 = openFile(spdFile, "r");
  aggInfo.numDays = 0;
  fscanf(in1, "%d %d", &year, &julianDay);
  leapYr = (year % 4 == 0);

  // find start point for comparisons:
  dataCount = 0;
  status1 = fscanf(in1, "%d", &spd);
  dataCount += spd;
  while (dataCount < startCompare && status1 != EOF) {
    status1 = fscanf(in1, "%d", &spd);
    dataCount += spd;
    julianDay++;
    if ((julianDay > 365 && !leapYr) || (julianDay > 366)) { // HAPPY NEW YEAR!
      julianDay = 1;
      year++;
      leapYr = (year % 4 == 0);
    }
  }
  aggInfo.startYear = year;
  aggInfo.startDay = julianDay;
  if (dataCount >= startCompare)
    aggInfo.numDays = 1; // we've found our first day
  // now find number of days:
  while (dataCount < endCompare && status1 != EOF) {
    status1 = fscanf(in1, "%d", &spd);
    dataCount += spd;
    aggInfo.numDays++;
  }

  fprintf(outFile, "Post-comparisons (in .agg file): start year = %d, start day = %d, # days = %d\n\n", 
	  aggInfo.startYear, aggInfo.startDay, aggInfo.numDays);

  aggInfo.spd = (int *)malloc(aggInfo.numDays * sizeof(int));

  rewind(in1); // go back to beginning
  fscanf(in1, "%*d %*d"); // read and ignore
  // re-find start point:
  dataCount = 0;
  status1 = fscanf(in1, "%d", &spd);
  dataCount += spd;
  while (dataCount < startCompare && status1 != EOF) {
    status1 = fscanf(in1, "%d", &spd);
    dataCount += spd;
  }
  // now fill spd array:
  if (dataCount >= startCompare) // if there's at least one day
    // account for possible missing steps in first day
    aggInfo.spd[0] = dataCount - startCompare + 1; 
  for (index = 1; index < aggInfo.numDays; index++) {
    fscanf(in1, "%d", &spd);
    aggInfo.spd[index] = spd;
    dataCount += spd;
  }
  aggInfo.spd[aggInfo.numDays - 1] -= (dataCount - endCompare); 
  // account for possible missing steps in last day
  
  fclose(in1);
  
  in1 = openFile(validFile, "r");
  for (index = 0; index < numData; index++) {
    fscanf(in1, "%lf", &thisValidFrac);
    valid[index] = (thisValidFrac >= validFrac);
  }
  fclose(in1);
}


/* for each changeable parameter, read parameter info into pInfo (a pointer to an array of PInfo's)
   and set next value in global indices array to the index of this parameter
   
   return number of changeable parameters
   
   numParams is total number of parameters (i.e. size of paramPtrs array)
   fileName.param gives parameter info, and each line is of the format:
   name  value  changeable  min  max  sigma
   where changeable = 1 if this parameter is changeable, 0 if not
*/
int readParamInfo(int numParams, PInfo **pInfo, char *fileName, FILE *outF) {
  int index; // index in file (0 = 1st line, 1 = 2nd line, etc.)
  int count; // count of changeable parameters
  char name[64];
  int changeable;
  double guess, min, max, sigma;
  FILE *in;
  char paramFile[64];

  strcpy(paramFile, fileName);
  strcat(paramFile, ".param");
  
  in = openFile(paramFile, "r");

  *pInfo = (PInfo *)malloc(numParams * sizeof(PInfo)); // this is the biggest these arrays can be
  indices = (int *)malloc(numParams * sizeof(int)); 
  
  count = 0;
  index = 0;
  fprintf(outF, "Changeable Parameters:\n");
  while ((fscanf(in, "%s %lf %d %lf %lf %lf", name, &guess, &changeable, &min, &max, &sigma)) != EOF) {
    // we have another parameter
    if (changeable) {
      fprintf(outF, "[%d] %s: Guess = %f\t Sigma = %f\n", count, name, guess, sigma);
      strcpy((*pInfo)[count].name, name);
      (*pInfo)[count].guess = guess;
      (*pInfo)[count].min = min;
      (*pInfo)[count].max = max;
      (*pInfo)[count].sigma = sigma;
      indices[count] = index;
      count++;
    }
    index++;
  }

  fprintf(outF, "\n");

  fclose(in);
  return count;
}

/* Write a new parameter file (newFile):
   It will mostly be the same as fileName.param, 
   except with new parameters given by params[0..np-1]
   params[i] is the parameter corresponding to indices[i]
*/
void writeNewParams(int np, double *params, char *fileName, char *newFile) {
  FILE *in, *out;
  char oldParams[64];
  int count, index;
  char name[64];
  int changeable;
  double value, min, max, sigma;

  strcpy(oldParams, fileName);
  strcat(oldParams, ".param");

  in = openFile(oldParams, "r");
  out = openFile(newFile, "w");

  count = index = 0;
  while ((fscanf(in, "%s %lf %d %lf %lf %lf", name, &value, &changeable, &min, &max, &sigma)) != EOF) {
    if (indices[index] == count) { // we're at the next changing parameter
      value = params[index];
      index++;
    }
    fprintf(out, "%s\t%f\t%d\t%f\t%f\t%f\n", name, value, changeable, min, max, sigma);
    count++;
  }

  fclose(in);
  fclose(out);
}


// call this when done program:
// free space used by global pointers
void cleanupParamchange() {
  free(data);
  free(sigmas);
  free(indices);
  free(valid);
  free(aggInfo.spd);
}

// Bill Sacks
// 7/9/02

// functions shared by different parameter estimation methods

#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "paramchange.h"
#include "util.h"

// the following variables are made global because they are computed once
// at the beginning of the program, and then must stick around (unchanging) for
// the whole program

static double numLocs; /* number of spatial locations: first dimension of data,
        startOpt, endOpt, valid, numAggSteps, aggSteps, aggedData, aggInfo set
        in readData */
static double ***data; /* read in once at start of program
       compared with model data in difference function
       1st dimension is spatial location, 2nd is time step, 3rd is data type */
static double ***sigmas; /* (dm) data uncertainty read in once at start of
       program compared with model data in difference function 1st dimension is
       spatial location, 2nd is time step, 3rd is data type */
static double **model;  // made global so don't have to re-allocate memory all
                        // the time
static int *startOpt, *endOpt;  // starting and ending indices for optimization
                                // (1-indexing) (vector: spatial)
static int ***valid; /* valid[i][j][k] indicates whether data[i][j][k] is valid
           (0 = invalid, non-0 = valid) (based on fraction of valid data points)
         */

static int *numAggSteps = NULL; /* size of 2nd dimension of aggSteps array
           (spatial) (explicitly initialized to NULL because we may never malloc
           this array) */
static int **aggSteps = NULL; /* array specifying how many steps make up each
         aggregated step (may be non-rectangular) 1st dimension is spatial
         location, 2nd is aggregated step (explicitly initialized to NULL
         because we may never malloc this array) */
static double ***aggedData = NULL; /* aggregated data (initialized to NULL b/c
             we may never malloc this array) only holds data between startOpt
             and endOpt */
static double **aggedModel = NULL; /* aggregated model (intitialized to NULL b/c
              we may never malloc this array) made global so don't have to
              re-allocate memory all the time only holds model output between
              startOpt and endOpt */
// Commenting out as all uses are commented out
// static double unaggedWeight = 0.0; /* if aggregation is done, weight of
//              unaggregated data in optimization (0 -> only use aggregated
//              data)
//            */

typedef struct AggregateInfoStruct {  // set at beginning of program
  int startPt, endPt;  // indices of data point to start and stop at
                       // (1-indexing)
  int *spd;  // steps per day
  int numDays;
  int startYear;  // 4 digit year
  int startDay;  // Julian day of year (1 = Jan. 1)
} AggregateInfo;

static AggregateInfo *aggInfo;  // vector: spatial

/* Difference, version 4 - READS IN SIGMAS (dm) ESTIMATES SIGMA, RETURNS
   AGGREGATE INFO
   Run modelF with given parameters at location loc, compare output with
   measured data using data types given by dataTypeIndices[0..numDataTypes-1]
   Return a measure of the difference between measured and predicted data
   (Higher = worse)
   Return best sigma value for each data type in sigma[0..numDataTypes-1]
   Return mean error, mean daily-aggregated error, and yearly-aggregated output
   for each year and each data type in *outputInfo array
   Pre: sigma and outputInfo are already malloced, as are outputInfo[*].years
   arrays

   Only use "valid" data points (as determined by validFrac in readData)
   And only use points between startOpt and endOpt (set in readData)
   [IGNORE paramWeight - just there to be consistent with old difference
   function]

   NOTE: this is actually the NEGATIVE log likelihood, discarding constant terms
   to get true log likelihood, add n*log(sqrt(2*pi)), then multiply by -1
*/
/*Added read in ***sigmas
 * removed ***sigma from double difference(double  *sigma, ***sigmas, OutputInfo
 * *outputInfo,
 *
 */
double difference(double *sigma, OutputInfo *outputInfo, int loc,
                  SpatialParams *spatialParams, double paramWeight,
                  void (*modelF)(double **, int, int *, SpatialParams *, int),
                  int dataTypeIndices[], int numDataTypes, int costFunction,
                  double dataTypeWeights[]) {
  int i, dataNum;
  double *sumSquares;  // one sum of squares value for each data type
  double *DownWtSumSquares;  // sum of square value downweighted by the number
                             // of data points _dm 05 24 10
  int *n;  // number of data points used in each sumSquares
  double logLike;  // the log likelihood
  double thisSigma;  //(dm) declare thisSigma
  // FILE *dbg; //(dm) debug file
  sumSquares = makeArray(numDataTypes);
  DownWtSumSquares = makeArray(numDataTypes);
  n = (int *)malloc(numDataTypes * sizeof(int));

  (*modelF)(model, numDataTypes, dataTypeIndices, spatialParams, loc);
  // run model, put results in model array

  // initialize sumSquares and count arrays

  for (dataNum = 0; dataNum < numDataTypes; dataNum++) {
    sumSquares[dataNum] = 0.0;
    DownWtSumSquares[dataNum] = 0.0;
    n[dataNum] = 0;
  }

  for (i = startOpt[loc] - 1; i < endOpt[loc]; i++) {
    for (dataNum = 0; dataNum < numDataTypes; dataNum++) {
      if (valid[loc][i][dataNum]) {

        if (costFunction == 0) {
          thisSigma = sqrt(0.5);  // set it to be the square root of 0.5 to
                                  // cancel out the expression below if we
                                  // aren't estimating sigma.
        } else {
          thisSigma = sigmas[loc][i][dataNum]; /* Now set it to be the sigma
                                                  value read in */
        }

        // output thissigma to screen to debug
        // fprintf(stdout,"thisSigma = %f\n\n", thisSigma);
        //  sigmas are read in and provide expected numbers but there are some
        //  missing data points??
        // output residual to the screen
        //	  	FILE *fp=stdout;
        //	  	stdout=fopen("out-snap","w");
        //	  	fprintf(stdout,"thisSigma = %f\n\n", thisSigma);
        // sometimes this results in the creation of an empty file - probably
        // indicating that the sigmas are not being read in correctly
        // fprintf(stdout,"Square residual = %f\n\n", (pow((model[i][dataNum] -
        // data[loc][i][dataNum]), 2))); printf("Hello hello");
        //	  	fclose(stdout);
        //	  	stdout=fp;

        sumSquares[dataNum] +=
            (pow((model[i][dataNum] - data[loc][i][dataNum]), 2) /
             (2.0 * thisSigma * thisSigma));

        n[dataNum]++;
      }
    }
  }
  //(removed dm) sumSquares[dataNum] += pow((model[i][dataNum] -
  // data[loc][i][dataNum]), 2);

  // calculate aggregate info on each data type
  for (dataNum = 0; dataNum < numDataTypes; dataNum++) {
    aggregates(outputInfo, model, loc, dataNum);
  }

  // Make sure we don't keep multiplying by zero for the product cost function
  if (costFunction == 3) {
    logLike = 1;
  } else {
    logLike = 0;
  }

  ////value of logLike;
  for (dataNum = 0; dataNum < numDataTypes; dataNum++) {
    sigma[dataNum] = sqrt(sumSquares[dataNum] / (double)(n[dataNum]));
    /* we can estimate sigma[i] using just sumSquares[i] because sigma[i] is
       calculated by taking the partial derivative of likelihood with respect to
       sigma[i], and this partial only depends on sumSquares[i]
       // sigma[dataNum]  is no longer used.  Instead sigma is read in for each
       observation as dot.sigmas
    */
    if (costFunction == 0) {
      // if n[dataNum] = 0, then the program will give an error here.  This is
      // just a test to make sure we are ok.
      if (n[dataNum] != 0) {
        logLike += dataTypeWeights[dataTypeIndices[dataNum]] * n[dataNum] *
                   log(sigma[dataNum]);
        logLike += dataTypeWeights[dataTypeIndices[dataNum]] *
                   sumSquares[dataNum] /
                   (2.0 * (sigma[dataNum]) * (sigma[dataNum]));
      }  // (n[dataNum] != 0
    }  // (costFunction == 0)
       //+(n[dataNum] * log(sigmas[dataNum]));// logLike is the sum of the
       // weighted sum of squares (j) for each data type
    else if (costFunction == 1) {
      logLike +=
          dataTypeWeights[dataTypeIndices[dataNum]] * sumSquares[dataNum];
    } else if (costFunction == 2) {
      logLike += numDataTypes * (sumSquares[dataNum] / (1 + n[dataNum]));
    } else if (costFunction == 3) {
      logLike *= pow(sumSquares[dataNum], (1.0 / numDataTypes));
    }

    //(CF2)
    // DownWtSumSquares[dataNum] =(sumSquares[dataNum]/(1+n[dataNum]));
    // //calculates the down-weighted sum of squares ***added 1 to avoid
    // division by zero?;
    //    	  logLike += ((DownWtSumSquares[dataNum])*3.0);//logLike is sum of
    //    the Cost Functions for each data type divided by the number of data
    //    points used for each (CF2)
    //(CF3)
    // logLike *=pow(sumSquares[dataNum],(1.0/5));//logLike is the product of
    // the weighted sum of squares (j) for each data type - ???plus one to avoid
    // a perfect fit causing the likelihood calculation to explode
    //????take the 7th root to bring back to a value similar to the individual
    // costfuntions CF3

    //
    // logLike += log(thisSigma);//(dm) now the individual sigmas for each data
    // point is added to the logLike // moved to line 101
    // sigmas are read in and provide expected numbers but there are some
    // missing data points??
    // output residual to the screen
    // FILE *fp=stdout;
    // stdout=fopen("out-snap","w");
    // fprintf(stdout,"Instant SumSquares = %f\n\n", sumSquares[dataNum]);
    // fprintf(stdout,"logLike = %f\n\n", logLike);
    // sometimes this results in the creation of an empty file - probably
    // indicating that the sigmas are not being read in correctly
    // fprintf(stdout,"Square residual = %f\n\n", (pow((model[i][dataNum] -
    // data[loc][i][dataNum]), 2))); printf("Hello hello"); fclose(stdout);

    // stdout=fp;
    // logLike += n[dataNum] * log(sigma[dataNum]);//change this - add to nested
    // loop logLike += sumSquares[dataNum]); //change this - add to nested loop
  }
  /*remove the division by 2 sigma^2 in the calculation of logLike, near the
   * bottom of the function (i.e., you are now doing this division once per
   * observation, rather than just once at the end)
   *
   * (dm removed:)
   *  (2.0*(sigma[dataNum])*(sigma[dataNum])
   */

  // debug;
  // dbg = openFile(outFileName, "w");
  // fprintf(stdout,"sumSquares = %f\n\n", sumSquares[dataNum]);
  // fprintf(dbg, "\n\n");
  // fclose(dbg);

  free(sumSquares);
  free(n);

  // NOTE: this is actually the NEGATIVE log likelihood, discarding constant
  // terms to get true log likelihood, add n*log(sqrt(2*pi)), then multiply by
  // -1

  return logLike;
}

/* Difference, version 3 - ESTIMATES SIGMA, RETURNS AGGREGATE INFO
   Run modelF with given parameters at location loc, compare output with
   measured data using data types given by dataTypeIndices[0..numDataTypes-1]
   Return a measure of the difference between measured and predicted data
   (Higher = worse)
   Return best sigma value for each data type in sigma[0..numDataTypes-1]
   Return mean error, mean daily-aggregated error, and yearly-aggregated output
   for each year and each data type in *outputInfo array Pre: sigma and
   outputInfo are already malloced, as are outputInfo[*].years arrays

   Only use "valid" data points (as determined by validFrac in readData)
   And only use points between startOpt and endOpt (set in readData)
   [IGNORE paramWeight - just there to be consistent with old difference
   function]

   NOTE: this is actually the NEGATIVE log likelihood, discarding constant terms
   to get true log likelihood, add n*log(sqrt(2*pi)), then multiply by -1
*/

/*dm
  double difference(double *sigma, OutputInfo *outputInfo,
      int loc, SpatialParams *spatialParams, double paramWeight,
      void (*modelF)(double **, int, int *, SpatialParams *, int),
      int dataTypeIndices[], int numDataTypes)
{
  int i, dataNum;
  double *sumSquares; // one sum of squares value for each data type
  int *n; // number of data points used in each sumSquares
  double logLike; // the log likelihood

  sumSquares = makeArray(numDataTypes);
  n = (int *)malloc(numDataTypes * sizeof(int));

  (*modelF)(model, numDataTypes, dataTypeIndices, spatialParams, loc);
  // run model, put results in model array

  // initialize sumSquares and count arrays
  for (dataNum = 0; dataNum < numDataTypes; dataNum++) {
    sumSquares[dataNum] = 0.0;
    n[dataNum] = 0;
  }

  for (i = startOpt[loc] - 1; i < endOpt[loc]; i++) {
    for (dataNum = 0; dataNum < numDataTypes; dataNum++) {
      if (valid[loc][i][dataNum]) {
  sumSquares[dataNum] += pow((model[i][dataNum] - data[loc][i][dataNum]), 2);
  n[dataNum]++;
      }
    }
  }

  // calculate aggregate info on each data type
  for (dataNum = 0; dataNum < numDataTypes; dataNum++)
    aggregates(outputInfo, model, loc, dataNum);

  logLike = 0;
  for (dataNum = 0; dataNum < numDataTypes; dataNum++) {
    sigma[dataNum] = sqrt(sumSquares[dataNum]/(double)(n[dataNum]));
    // we can estimate sigma[i] using just sumSquares[i] because sigma[i] is
    // calculated by taking the partial derivative
    // of likelihood with respect to sigma[i], and this partial only depends
    // on sumSquares[i]
    */

/* (dm)
logLike += n[dataNum] * log(sigma[dataNum]);
    logLike += sumSquares[dataNum]/(2.0*(sigma[dataNum])*(sigma[dataNum]));
  }

  free(sumSquares);
  free(n);

  // NOTE: this is actually the NEGATIVE log likelihood, discarding constant
  // terms to get true log likelihood, add n*log(sqrt(2*pi)), then multiply
  // by -1

  return logLike;
}
 */

/* pre: **origArray contains original (unaggregated) data/model output
   (origArray[i][j] is timestep i, data type j)
   myNumAggSteps is number of aggregated time steps
   myAggSteps is a vector [0..myNumAggSteps-1] containing number of time steps
   in each aggregated steps myStartOpt is index of first time step used in
   optimization (1-indexed) Note that myNumAggSteps, myAggSteps and myStartOpt
   are taken from global variables at a single spatial location

   aggregate data/model output (sum across each aggregated time step)
   and return result in theAggedData
*/
void computeAggedData(double **theAggedData, double **origArray,
                      int myNumAggSteps, int *myAggSteps, int myStartOpt,
                      int numDataTypes) {
  int i, j, dataType, index;

  index = myStartOpt - 1;  // index into data array (myStartOpt is 1-indexed)
  for (i = 0; i < myNumAggSteps; i++) {
    for (dataType = 0; dataType < numDataTypes; dataType++) {
      theAggedData[i][dataType] = 0.0;  // reset sums
    }

    for (j = 0; j < myAggSteps[i]; j++) {  // loop through all steps in this
                                           // aggregated step
      for (dataType = 0; dataType < numDataTypes; dataType++) {
        // accumulate each sum
        theAggedData[i][dataType] += origArray[index][dataType];
      }
      index++;
    }
  }
}

/* Aggregated difference - ESTIMATES SIGMA, RETURNS AGGREGATE INFO
   Same as difference function above, but aggregates model output to fewer steps
   Total difference is a weighted sum of error on aggregated output vs. data
   and error on unaggregated output vs. data (weight determined by global
   unaggedWeight)

   Run modelF with given parameters at location loc, compare output with
   measured data using data types given by dataTypeIndices[0..numDataTypes/2-1]
   Return a measure of the difference between measured and predicted data
   (Higher = worse)
   Assumes error is the same on each aggregated time step,
   even if aggregation lengths are different
   Return best sigma value for each data type in sigma[0..numDataTypes-1]
   (where sigma[i] is sigma for data type given by
   dataTypeIndices[i%(numDataTypes/2)]; if i < numDataTypes/2 then sigma[i]
   gives sigma for unaggregated data; otherwise for aggregated data) Return mean
   error, mean daily-aggregated error, and yearly-aggregated output for each
   year and each data type in *outputInfo array (indexed like sigma array - see
   above)

   Pre: sigma and outputInfo are already malloced, as are outputInfo[*].years
   arrays global *numAggSteps, **aggSteps and ***aggedData have all been set
   appropriately, and global **aggedModel has been malloced appropriately
   numDataTypes is actually TWICE the number of data types
   (for each data type, one unaggregated and one aggregated)

   FOR NOW, WE IGNORE THE VALID FRACTION (THIS HASN'T BEEN AGGREGATED UP)
   Only use points between startOpt and endOpt (set in readData)
   (NOTE: UNTESTED FOR ANYTHING BUT STARTOPT=1, ENDOPT=-1 (I.E. ALL POINTS))
   [IGNORE paramWeight - just there to be consistent with difference function
   above]
*/

// Apr-25-2025: clang-tidy asserts that there are issues with this function,
// and it is not currently being used. Disabling for now, not worth
// spending time on.
// 6/9/11: For now we don't do anything with the cost function or data Type
// weights - we just want to make it consistent with the difference function
// above so we don't get an error.
#if 0
double aggedDifference(double *sigma, OutputInfo *outputInfo, int loc,
                       SpatialParams *spatialParams, double paramWeight,
                       void (*modelF)(double **, int, int *, SpatialParams *,
                                      int),
                       int dataTypeIndices[], int numDataTypes,
                       int costFunction, double dataTypeWeights[]) {
  int i, dataNum;
  double *sumSquares;  // one sum of squares value for each data type
  int *n;  // number of data points used in each sumSquares
  double logLike;  // the log likelihood

  sumSquares = makeArray(numDataTypes);
  n = (int *)malloc(numDataTypes * sizeof(int));

  (*modelF)(model, numDataTypes / 2, dataTypeIndices, spatialParams, loc);
  // run model, put results in model array
  // divide numDataTypes by 2 so we have actual (unbifurcated) number of data
  // types

  // aggregate model, filling aggedModel array
  computeAggedData(aggedModel, model, numAggSteps[loc], aggSteps[loc],
                   startOpt[loc], numDataTypes / 2);

  // again, divide numDataTypes by 2 to give actual (unbifurcated) number of
  // data types

  // initialize sumSquares and count arrays
  for (dataNum = 0; dataNum < numDataTypes; dataNum++) {
    sumSquares[dataNum] = 0.0;
    n[dataNum] = 0;
  }

  // compute sum of squares on unaggregated data:
  for (i = startOpt[loc] - 1; i < endOpt[loc]; i++) {
    for (dataNum = 0; dataNum < numDataTypes / 2; dataNum++) {
      if (valid[loc][i][dataNum]) {
        sumSquares[dataNum] +=
            pow((model[i][dataNum] - data[loc][i][dataNum]), 2);
        n[dataNum]++;
      }
    }
  }

  // compute sum of squares on aggregated data (note: we don't check validity
  // here - instead use all points):
  for (i = 0; i < numAggSteps[loc]; i++) {
    for (dataNum = 0; dataNum < numDataTypes / 2; dataNum++) {
      sumSquares[numDataTypes / 2 + dataNum] +=
          pow((aggedModel[i][dataNum] - aggedData[loc][i][dataNum]), 2);
      n[numDataTypes / 2 + dataNum]++;
    }
  }

  // calculate aggregate info on each data type
  for (dataNum = 0; dataNum < numDataTypes / 2; dataNum++) {
    aggregates(outputInfo, model, loc, dataNum);

    // copy aggregate info from position i to position (numDataTypes/2 + i)
    // (same data type, but aggregated - will have same aggregate info)
    copyOutputInfo(&(outputInfo[numDataTypes / 2 + dataNum]),
                   &(outputInfo[dataNum]));
  }

  // compute log likelihood (actually, negative log likelihood, discarding
  // constant terms):
  logLike = 0;
  for (dataNum = 0; dataNum < numDataTypes; dataNum++) {
    sigma[dataNum] = sqrt(sumSquares[dataNum] / (double)(n[dataNum]));
    /* we can estimate sigma[i] using just sumSquares[i] because sigma[i] is
       calculated by taking the partial derivative of likelihood with respect to
       sigma[i], and this partial only depends on sumSquares[i]
    */
    logLike += n[dataNum] * log(sigma[dataNum]);
    logLike +=
        sumSquares[dataNum] / (2.0 * (sigma[dataNum]) * (sigma[dataNum]));

    if (dataNum == numDataTypes / 2 - 1) {
      // we just finished all unaggregated types, so apply unaggedWeight to
      // current log likelihood
      logLike *= unaggedWeight;
    }
  }

  free(sumSquares);
  free(n);

  // NOTE: this is actually the NEGATIVE log likelihood, discarding constant
  // terms

  return logLike;
}
#endif

/* Take array of model output, compare output with measured data - for given
   dataNum (i.e. perform comparisons between model[*][dataNum] and
   data[loc][*][dataNum] Return (in outputInfo[dataNum]) mean error (per day),
   mean daily-aggregated error (per day) Also fill years array with annual total
   NEE for each year and set outputInfo.numYears, the size of the years array
   Pre: outputInfo is already malloced, as are outputInfo[*].years arrays
   (Can later use years array to find yearly-aggregated error and interannual
   variability)
*/
void aggregates(OutputInfo *outputInfo, double **localModel, int loc,
                int dataNum) {
  const int DAYS_IN_YR[] = {365, 366}; /* given leapYr = 0 or 1,
                DAYS_IN_YR[leapYr] represents the number of days in this year */
  int i, j, day, julianDay, year;
  int leapYr;  // 1 if this is a leap year
  int index;
  double *modelD, *dataD;  // daily aggregations
  int yearIndex;  // starting at 0 rather than being a 4-digit year
  double sum;
  double netNeeM, netNeeD;  // net model and data nee for this aggregated step

  modelD = makeArray(aggInfo[loc].numDays);
  dataD = makeArray(aggInfo[loc].numDays);

  sum = 0.0;
  for (i = aggInfo[loc].startPt - 1; i < aggInfo[loc].endPt; i++) {
    sum += fabs(localModel[i][dataNum] - data[loc][i][dataNum]);
  }

  outputInfo[dataNum].meanError = sum / aggInfo[loc].numDays;  // mean error per
                                                               // day

  // fill daily aggregation arrays, compute error aggregated over days:
  sum = 0.0;
  index = aggInfo[loc].startPt - 1;
  for (day = 0; day < aggInfo[loc].numDays; day++) {
    netNeeM = netNeeD = 0.0;
    for (j = 0; j < aggInfo[loc].spd[day]; j++) {
      netNeeM += localModel[index][dataNum];
      netNeeD += data[loc][index][dataNum];
      index++;
    }
    modelD[day] = netNeeM;
    dataD[day] = netNeeD;
    sum += fabs(netNeeM - netNeeD);
  }

  // set mean daily-aggregated error per day
  outputInfo[dataNum].daysError = sum / aggInfo[loc].numDays;

  // now do yearly and overall aggregation:

  // first fill year arrays and find yearly aggregation:
  day = 0;
  julianDay = aggInfo[loc].startDay;
  year = aggInfo[loc].startYear;
  leapYr = (year % 4 == 0);  // holds for 1900 < year < 2100
  yearIndex = 0;
  while (day < aggInfo[loc].numDays) {
    netNeeM = 0.0;
    while (julianDay <= DAYS_IN_YR[leapYr] && day < aggInfo[loc].numDays) {
      // loop through current year
      netNeeM += modelD[day];
      day++;
      julianDay++;
    }
    // HAPPY NEW YEAR!

    outputInfo[dataNum].years[yearIndex] = netNeeM;  // NEE over this whole year
    // can later use this to find mean yearly-aggregated error per day and
    // interannual variability

    julianDay = 1;
    year++;
    yearIndex++;
    leapYr = (year % 4 == 0);  // holds for 1900 < year < 2100
  }
  outputInfo[dataNum].numYears = yearIndex;  // the actual number of years

  free(modelD);
  free(dataD);
}

// read one line of data from in to arr[0..numDataTypes - 1]
void readDataLine(FILE *in, double *arr, int numDataTypes) {
  char line[256];
  char *remainder;  // after each read, remainder points to start of remainder
                    // of line
  int i;

  fgets(line, sizeof(line), in);
  remainder = line;
  for (i = 0; i < numDataTypes; i++) {
    // put next double in arr[i], set remainder to point to remainder of string:
    arr[i] = strtod(remainder, &remainder);
  }

  // printf("%f\n", arr[numDataTypes-1]);
}

// count & return number of lines in given file
// (stop counting as soon as reach end of file or a blank line)
int countLines(char *fileName) {
  FILE *f;
  char line[256];
  int numLines;

  f = openFile(fileName, "r");
  numLines = 0;
  while ((fgets(line, sizeof(line), f) != NULL) && (strcmp(line, "\n") != 0) &&
         (strcmp(line, "\r\n") != 0)) {
    // read & ignore
    numLines++;
  }

  fclose(f);

  return numLines;
}

/* Parse one string (spdString) from .spd file
   Three possible options:
   - If spdString is "-1", return -1, leave spd unchanged, set count to 0
   - If spd is "#n", where n is a positive integer (e.g. "#100"), return 0,
   don't change spd (we'll use last spd value), set count to n (this signifies
   that we should repeat previous value n times)
   - Otherwise, spd must be a non-negative integer (in string format): return 1,
   set spd to the number, set count to 1
*/
int parseSpdValue(char *spdString, int *spd, int *count) {
  char *errc;

  if (strcmp(spdString, "-1") == 0) {
    return -1;
  }

  if (spdString[0] == '#') {  // spd is "#n" (e.g. #100)
    // get rid of '#', convert to integer
    *count = strtol(strtok(spdString, "#"), &errc, 0);
    return 0;
  }

  // spd is an integer (in string format)
  *spd = strtol(spdString, &errc, 0);
  *count = 1;
  return 1;
}

/* Read indices from fileName, put into startIndices and endIndices vectors
   (which have already been malloc'ed: vectors of size numLocs)
   Format of file: numLocs lines, each line contains two integers: start & end
   end = -1 signifies go to end, in which case set end[i] = steps[i]
   (steps is an array[0..numLocs-1] giving # of steps at each location)
   if end[i] > steps[i], endIndices[i] set equal to steps[i]
   FileName can be empty string (""), in which case all startIndices are set to
   1, and endIndices set equal to steps
*/
void readIndicesFile(char *fileName, int *startIndices, int *endIndices,
                     /*int numLocs,*/ int *steps) {
  FILE *in;
  int loc;
  int noFile = 1;  // start by assuming there's no file

  if (strcmp(fileName, "") != 0) {  // there's a file
    in = openFile(fileName, "r");
    noFile = 0;  // false: there IS a file
  }

  for (loc = 0; loc < numLocs; loc++) {
    if (noFile) {
      startIndices[loc] = 1;
      endIndices[loc] = steps[loc];
    } else {
      fscanf(in, "%d %d", &(startIndices[loc]), &(endIndices[loc]));
      if (endIndices[loc] == -1 || endIndices[loc] > steps[loc]) {
        endIndices[loc] = steps[loc];
      }
    }
  }

  if (!noFile) {
    // we opened a file
    fclose(in);
  }
}

// Apr-25-2025: clang-tidy asserts that there are issues with this function,
// and it is not currently being used. Disabling for now, not worth
// spending time on.
/* Read measured data (from fileName.dat) and valid fractions (from
   fileName.valid) into arrays (used to also read sigmas) and set values in
   valid array (based on validFrac) Each line in data (and valid) file has
   totNumDataTypes columns and each file has one line for each time step at each
   of the myNumLocs location (all time steps for a single location are
   continuous), with NO blank lines where steps vector gives # of time steps at
   each location (so total number of lines in data/valid files should equal sum
   of elements in steps vector) dataTypeIndices give the numDataTypes indices of
   the data types that we'll actually use in optimization Also read spd file
   (steps per day) (fileName.spd), which has one line for each location Each
   line begins with the year of the first point, followed by the julian day of
   the first point, followed by the number of steps per day for each day,
   terminated by -1 As a short-hand, #n means repeat the last steps-per-day
   value n more times (e.g. "2 #3" is equivalent to "2 2 2 2")

   Also possibly read start and end indices for optimizations for each location
   (from optIndicesFile), and start and end indices for final model-data
   comparisons for each location (from compareIndicesFile), where these files
   have one line for each location, and each line contains two integers: start &
   end; if either of these file names is given as an empty string (""), use all
   data points for optimizations or model-data comparisons. NOTE: Both of these
   use 1-indexing; end = -1 means go to end

   steps is an array giving the number of time steps at each of the myNumLocs
   locations

   This function also allocates space for model array
*/
#if 0
void readData(char *fileName, int dataTypeIndices[], int numDataTypes,
              int totNumDataTypes, int myNumLocs, int *steps, double validFrac,
              char *optIndicesFile, char *compareIndicesFile, FILE *outFile) {
  char dataFile[64], spdFile[64], validFile[64],
      sigmaFile[64];  //(dm) uncommented sigmaFile[64];
  FILE *in1, *in2, *in3;
  int status;
  int index;
  int julianDay;
  int year, leapYr;
  int spd, startSpd, dataCount, startDataCount;
  double *oneLine;  // data from one line of a file
  int totSteps;  // total number of time steps (sum over all locations)
  int maxSteps;  // maximum number of time steps at any location
  int numData;
  int i, loc;
  char spdString[32];  // store one # from spd file (in a string to allow for
                       // shorthands like #<n>)
  int count, startCount;
  long filePos;
  int *tempStartCompare, *tempEndCompare;  // vectors holding compare indices
                                           // temporarily, before they're put in
                                           // aggInfo

  numLocs = myNumLocs;  // set global numLocs

  strcpy(dataFile, fileName);
  strcpy(sigmaFile, fileName);  //(dm) uncommented
  strcpy(spdFile, fileName);
  strcpy(validFile, fileName);
  strcat(dataFile, ".dat");
  strcat(sigmaFile, ".sigma");  //(dm)uncommented
  strcat(spdFile, ".spd");
  strcat(validFile, ".valid");

  totSteps = 0;
  maxSteps = 0;
  for (loc = 0; loc < numLocs; loc++) {
    totSteps += steps[loc];  // count total number of data points expected
    if (steps[loc] > maxSteps) {
      maxSteps = steps[loc];
    }
  }
  numData = countLines(dataFile);  // determine number of data points in file
  if (totSteps != numData) {
    printf("Error: expected to read %d data points from file, read %d data "
           "points\n",
           totSteps, numData);
    exit(1);
  }

  in1 = openFile(dataFile, "r");
  //(dm) removed in2 = openFile(sigmaFile, "r");
  in2 = openFile(validFile, "r");
  in3 = openFile(sigmaFile, "r");  // better to add sigmaFile as in3 as in2 is
                                   // already assigned to validFile

  data = (double ***)malloc(numLocs * sizeof(double **));
  for (loc = 0; loc < numLocs; loc++) {
    // make 2-d array just big enough for known # of time steps in this location
    data[loc] = make2DArray(steps[loc], numDataTypes);
  }
  model = make2DArray(numData, numDataTypes);

  //  (dm) added code to read sigmas for each time step and each data type
  sigmas = (double ***)malloc(numLocs * sizeof(double **));
  for (loc = 0; loc < numLocs; loc++) {
    // make 2-d array just big enough for known # of time steps in this location
    sigmas[loc] = make2DArray(steps[loc], numDataTypes);
  }

  valid = (int ***)malloc(numLocs * sizeof(int **));
  for (loc = 0; loc < numLocs; loc++) {
    // make 2-d array just big enough for known # of time steps in this location
    valid[loc] = make2DIntArray(numData, numDataTypes);
  }
  oneLine = makeArray(totNumDataTypes);

  for (loc = 0; loc < numLocs; loc++) {
    for (index = 0; index < steps[loc]; index++) {
      readDataLine(in1, oneLine, totNumDataTypes);
      // assign data elements appropriately, based on which data types we're
      // using:
      for (i = 0; i < numDataTypes; i++) {
        data[loc][index][i] = oneLine[dataTypeIndices[i]];
      }

      readDataLine(in2, oneLine, totNumDataTypes);  // read valid file
      // assign valid elements appropriately, based on which data types we're
      // using:
      for (i = 0; i < numDataTypes; i++) {
        valid[loc][index][i] = (oneLine[dataTypeIndices[i]] >= validFrac);
      }

      readDataLine(in3, oneLine, totNumDataTypes);  // read sigmas file
      // assign sigmas elements appropriately, based on which data types we're
      // using:
      for (i = 0; i < numDataTypes; i++) {
        sigmas[loc][index][i] = oneLine[dataTypeIndices[i]];
      }
    }
  }

  free(oneLine);
  fclose(in1);
  fclose(in2);
  fclose(in3);

  startOpt = (int *)malloc(numLocs * sizeof(int));
  endOpt = (int *)malloc(numLocs * sizeof(int));
  readIndicesFile(optIndicesFile, startOpt, endOpt, steps);

  // now find start day, start year, and steps per day

  aggInfo = (AggregateInfo *)malloc(numLocs * sizeof(AggregateInfo));

  tempStartCompare = (int *)malloc(numLocs * sizeof(int));
  tempEndCompare = (int *)malloc(numLocs * sizeof(int));
  readIndicesFile(compareIndicesFile, tempStartCompare, tempEndCompare, steps);
  for (loc = 0; loc < numLocs; loc++) {
    aggInfo[loc].startPt = tempStartCompare[loc];
    aggInfo[loc].endPt = tempEndCompare[loc];
  }
  free(tempStartCompare);
  free(tempEndCompare);

  in1 = openFile(spdFile, "r");
  for (loc = 0; loc < numLocs; loc++) {
    aggInfo[loc].numDays = 0;

    if (fscanf(in1, "%d %d", &year, &julianDay) == EOF) {  // read year and
                                                           // julianDay (make
                                                           // sure there's
                                                           // something to
                                                           // read!)
      printf("Error: unexpected EOF trying to read loc #%d from spd file\n",
             loc);
      exit(1);
    }
    leapYr = (year % 4 == 0);  // this holds for 1900 < year < 2100

    // find start point for comparisons:
    dataCount = 0;
    fscanf(in1, "%s", spdString);
    status = parseSpdValue(spdString, &spd, &count);
    if (status != 1) {  // read -1 or #<n>: this is an error for first value
      printf("Error: read -1 or #<n> for first spd value\n");
      exit(1);
    }
    dataCount += spd;
    count--;  // decrement count to note that we stored the spd value (now count
              // will be 0)
    while (dataCount < aggInfo[loc].startPt) {
      fscanf(in1, "%s", spdString);
      status = parseSpdValue(spdString, &spd, &count);
      if (status == -1) {  // error: we've reached end of data before finding
                           // start position!
        printf("Error reading spd file: reached end of data before finding "
               "start position\n");
        exit(1);
      }

      while (count > 0 && dataCount < aggInfo[loc].startPt) {  // add correct
                                                               // number of
                                                               // spd's, but
                                                               // terminate if
                                                               // dataCount
                                                               // becomes >=
                                                               // startCompare
        dataCount += spd;
        julianDay++;
        if ((julianDay > 365 && !leapYr) || (julianDay > 366)) {  // HAPPY NEW
                                                                  // YEAR!
          julianDay = 1;
          year++;
          leapYr = (year % 4 == 0);  // holds for 1900 < year < 2100
        }  // if

        count--;  // decrement count: we've added one spd.
      }  // while
    }  // outer while
    // NOTE: may still have count > 0 if we terminated while in the middle of
    // processing a #n; will deal with this in just a bit

    // save position, dataCount, and remaining count and last spd read: we'll
    // need this later, when rewind file and re-read spd's
    filePos = ftell(in1);  // get current position so we can rewind to here
                           // later

    startCount = count;
    startSpd = spd;
    startDataCount = dataCount;

    aggInfo[loc].startYear = year;
    aggInfo[loc].startDay = julianDay;
    aggInfo[loc].numDays = 1;  // we've found our first day

    // now find number of days:

    if (count == 0) {  // we did NOT stop above while loop in the middle of
                       // going through a #<n>
      fscanf(in1, "%s", spdString);
      status = parseSpdValue(spdString, &spd, &count);
    }
    // if count > 0, we just leave count and spd as they are - as if we just
    // read #count
    while (dataCount < aggInfo[loc].endPt && status != -1) {
      while (count > 0 && dataCount < aggInfo[loc].endPt) {
        // add correct number of spd's, but terminate if dataCount becomes >=
        // endCompare
        dataCount += spd;
        aggInfo[loc].numDays++;
        count--;
      }  // while

      fscanf(in1, "%s", spdString);
      status = parseSpdValue(spdString, &spd, &count);
    }  // outer while

    if (dataCount < aggInfo[loc].endPt) {  // we reached status == -1 before
                                           // finding the end!
      printf("Error reading spd file: reached end of data before dataCount "
             "reached end location\n");
      exit(1);
    }

    fprintf(outFile,
            "Location #%d: Post-comparisons: start year = %d, start day = %d, "
            "# days = %d\n\n",
            loc, aggInfo[loc].startYear, aggInfo[loc].startDay,
            aggInfo[loc].numDays);

    aggInfo[loc].spd = (int *)malloc(aggInfo[loc].numDays * sizeof(int));

    fseek(in1, filePos, SEEK_SET); /* rewind to just before first spd value
              after any we skipped to get to startCompare note: we have skipped
              one extra spd string - but this is recorded in saved values
              ("start*" variables) SEEK_SET makes offset from start of file;
              filePos is set above */
    /* restore dataCount, count and spd from 1st pass through line
       These were set when we were at position given by filePos
       Note: we may have to deal with count > 0 */

    dataCount = startDataCount;
    count = startCount;
    spd = startSpd;

    // accounts for possible missing steps in first day
    aggInfo[loc].spd[0] = dataCount - aggInfo[loc].startPt + 1;

    if (count == 0) {  // we did NOT stop 1st pass in the middle of going
                       // through a #n
      fscanf(in1, "%s", spdString);
      status = parseSpdValue(spdString, &spd, &count);
    }
    // if count > 0, we just leave count and spd as they are - as if we just
    // read #count

    // now fill spd array:
    index = 1;
    while (index < aggInfo[loc].numDays) {
      while (count > 0 && index < aggInfo[loc].numDays) {
        // while loop needed to process #n possibility
        aggInfo[loc].spd[index] = spd;
        dataCount += spd;
        index++;
        count--;
      }  // while

      fscanf(in1, "%s", spdString);
      status = parseSpdValue(spdString, &spd, &count);
    }  // outer while

    aggInfo[loc].spd[aggInfo[loc].numDays - 1] -=
        (dataCount - aggInfo[loc].endPt);  // account for possible missing steps
                                           // in last day

    while (status != -1) {  // read values until we hit the -1 marking end of
                            // this location (if we haven't already)
      fscanf(in1, "%s", spdString);
      status = parseSpdValue(spdString, &spd, &count);
    }
  }  // for (loc)

  fclose(in1);
}
#endif

// Apr-25-2025: clang-tidy asserts that there are issues with this function,
// and it is not currently being used. Disabling for now, not worth
// spending time on.
/* pre: readData has been called (to set global startOpt, endOpt and numLocs
   appropriately)

   read number of time steps per each model-data aggregation from file
   each line in fileForAgg contains aggregation info for one location,
   where each value (i) on a given line is an integer giving the number of time
   steps in aggregation i in that time step (e.g. to run model-data comparison
   on a yearly aggregation, each value would be number of steps in year i); each
   line must be terminated with a -1

   if sum of all time steps (i.e. sum of all numbers on line) differs from total
   number of data, exit with an error message

   otherwise,
   set global numAggSteps (1-d array: spatial) and aggSteps (2-d array: spatial)
   and unaggedWeight appropriately also compute aggedData array (of size numLocs
   x numAggSteps x numDataTypes)
*/
#if 0
void readFileForAgg(char *fileForAgg, int numDataTypes,
                    double myUnaggedWeight) {
  FILE *f;
  int curr, sum, count, maxCount;
  int i, loc;
  long filePos;  // so we can rewind to a previous location in the file

  numAggSteps = (int *)malloc(numLocs * sizeof(int));
  aggSteps = (int **)malloc(numLocs * sizeof(int *));
  aggedData = (double ***)malloc(numLocs * sizeof(double **));

  f = openFile(fileForAgg, "r");

  maxCount = 0;
  for (loc = 0; loc < numLocs; loc++) {
    // first, find number of agged steps in this location:
    filePos = ftell(f);  // save current location
    count = 0;
    fscanf(f, "%d", &curr);
    while (curr != -1) {  // count number of values before next -1
      count++;
      fscanf(f, "%d", &curr);
    }
    numAggSteps[loc] = count;
    aggSteps[loc] = (int *)malloc(count * sizeof(int));
    if (count > maxCount) {
      maxCount = count;
    }

    // now fill aggSteps[loc] vector
    fseek(f, filePos, SEEK_SET);  // rewind to beginning of this location
                                  // (SEEK_SET makes offset from start of file)
    sum = 0;  // keep count of aggSteps to make sure we reach correct total
    for (i = 0; i < numAggSteps[loc]; i++) {  // read each piece of data
      fscanf(f, "%d", &curr);
      aggSteps[loc][i] = curr;
      sum += curr;
    }
    fscanf(f, "%d", &curr);  // read final "-1"

    if (sum != (endOpt[loc] - startOpt[loc] + 1)) {
      printf("Error: total number of data points specified in %s for location "
             "%d (%d) differs from number implied by\n",
             fileForAgg, loc, sum);
      printf("startOpt = %d and endOpt = %d (%d)\n", startOpt[loc], endOpt[loc],
             (endOpt[loc] - startOpt[loc] + 1));
      exit(1);
    }

    aggedData[loc] = make2DArray(numAggSteps[loc], numDataTypes);
    computeAggedData(aggedData[loc], data[loc], numAggSteps[loc], aggSteps[loc],
                     startOpt[loc], numDataTypes);
  }  // for (loc)

  fclose(f);

  aggedModel = make2DArray(maxCount, numDataTypes);
  unaggedWeight = myUnaggedWeight;
}
#endif

// malloc space for outputInfo array[0..numDataTypes-1], and outputInfo[*].years
// arrays for a single location, loc make years arrays large enough to hold data
// from given location
OutputInfo *newOutputInfo(int numDataTypes, int loc) {
  OutputInfo *outputInfo;
  int years;
  int i;

  outputInfo = (OutputInfo *)malloc(numDataTypes * sizeof(OutputInfo));

  years = aggInfo[loc].numDays / 365 + 2;  // this is the most years that could
                                           // be represented by this number of
                                           // days
  for (i = 0; i < numDataTypes; i++) {
    outputInfo[i].years = makeArray(years);
  }

  return outputInfo;
}

// copy data from outputInfo ptr in to outputInfo ptr out
// PRE: outputInfo struct pointed to by out has already been malloced, as has
// its years array
void copyOutputInfo(OutputInfo *out, OutputInfo *in) {
  int i;

  out->meanError = in->meanError;
  out->daysError = in->daysError;
  out->numYears = in->numYears;

  for (i = 0; i < out->numYears; i++) {
    out->years[i] = in->years[i];
  }
}

// given an outputInfo array[0..numDataTypes-1] (dynamically allocated), free
// years and the array itself
void freeOutputInfo(OutputInfo *outputInfo, int numDataTypes) {
  int i;

  for (i = 0; i < numDataTypes; i++) {
    free(outputInfo[i].years);
  }
  free(outputInfo);
}

// call this when done program:
// free space used by global pointers
void cleanupParamchange(void) {
  int loc;

  for (loc = 0; loc < numLocs; loc++) {
    free2DArray((void **)data[loc]);
  }
  free(data);

  free2DArray((void **)model);

  free(startOpt);
  free(endOpt);

  for (loc = 0; loc < numLocs; loc++) {
    free2DArray((void **)valid[loc]);
  }
  free(valid);

  // JZ ADD: Free sigma values
  for (loc = 0; loc < numLocs; loc++) {
    free2DArray((void **)sigmas[loc]);
  }
  free(sigmas);

  for (loc = 0; loc < numLocs; loc++) {
    free(aggInfo[loc].spd);
  }
  free(aggInfo);

  if (numAggSteps != NULL) {
    // we've malloced it
    free(numAggSteps);
  }

  if (aggSteps != NULL) {  // we've malloced it
    for (loc = 0; loc < numLocs; loc++) {
      free(aggSteps[loc]);
    }
    free(aggSteps);
  }

  if (aggedData != NULL) {  // we've malloced it
    for (loc = 0; loc < numLocs; loc++) {
      free2DArray((void **)aggedData[loc]);
    }
    free(aggedData);
  }

  if (aggedModel != NULL) {
    // we've malloced it
    free2DArray((void **)aggedModel);
  }
}

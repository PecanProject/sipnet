

/* Difference, version 3 - ESTIMATES SIGMA, RETURNS AGGREGATE INFO
   Run modelF with given parameters at location loc, compare output with measured data
   using data types given by dataTypeIndices[0..numDataTypes-1]
   Return a measure of the difference between measured and predicted data
   (Higher = worse)
   Return best sigma value for each data type in sigma[0..numDataTypes-1]
   Return mean error, mean daily-aggregated error, and yearly-aggregated output for each year
   and each data type in *outputInfo array
   Pre: sigma and outputInfo are already malloced, as are outputInfo[*].years arrays

   Only use "valid" data points (as determined by validFrac in readData)
   And only use points between startOpt and endOpt (set in readData)
   [IGNORE paramWeight - just there to be consistent with old difference function]

   NOTE: this is actually the NEGATIVE log likelihood, discarding constant terms
   to get true log likelihood, add n*log(sqrt(2*pi)), then multiply by -1
*/
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
    /* we can estimate sigma[i] using just sumSquares[i] because sigma[i] is calculated by taking the partial derivative
       of likelihood with respect to sigma[i], and this partial only depends on sumSquares[i]
    */
    logLike += n[dataNum] * log(sigma[dataNum]);
    logLike += sumSquares[dataNum]/(2.0*(sigma[dataNum])*(sigma[dataNum]));
  }

  free(sumSquares);
  free(n);

  // NOTE: this is actually the NEGATIVE log likelihood, discarding constant terms
  // to get true log likelihood, add n*log(sqrt(2*pi)), then multiply by -1
  
  return logLike;
}

/*9/14/04 modified by Bill Sacks to work with spatially-varying parameters */
/*7/15/02 modified by Bill Sacks to work with sipnet
  (original provided by George Hurtt) */
/*11/19/01 modified for new input*/
/*9/26/01 Program to estimate parameters in miami-biomass model*/


#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <float.h>
#include "paramchange.h"
#include "spatialParams.h"
#include "util.h"

#define A_STAR 0.4 // target acceptance rate
#define DEC 0.99 // how much to decrease temp. by on rejection
#define THRESH 0.02 // how close we have to get to A_STAR before stop adjusting temperatures


// write a header line for the .hist file
void writeHistFileHeader(FILE *histFile, int numYears, int np, int numDataTypes) {
  fprintf(histFile, "Log_likelihood  %dx(Sigma_estimate  Mean_error  Daily_aggregated_mean_error  %dxAnnual_Totals)  %dxParam_values\n",
	  numDataTypes, numYears, np);
}

// write one point to the .hist file for the given location (loc)
void writeHistFile(FILE *histFile, double ltotnew, double *sigma, OutputInfo *outputInfo, int numDataTypes, SpatialParams *spatialParams, int loc) {
  int i, j;
  int np; // number of changeable parameters 

  fprintf(histFile,"%f ",ltotnew);
  for (i = 0; i < numDataTypes; i++) {
    fprintf(histFile, "%f ", sigma[i]);
    fprintf(histFile, "%f %f ", outputInfo[i].meanError, outputInfo[i].daysError);
    for (j = 0; j < outputInfo[i].numYears; j++)
      fprintf(histFile, "%f ", outputInfo[i].years[j]); // annual output for this year
  }

  np = spatialParams->numChangeableParams;
  for (i = 0; i < np; i++)
    fprintf(histFile,"%f ", getSpatialParam(spatialParams, spatialParams->changeableParamIndices[i], loc));
  fprintf(histFile, "\n");
}


// .hist file writing, binary version:

// write a header for the .hist file in binary format (a single int giving number of floats per point)
void writeHistFileBinHeader(FILE *histFile, int numYears, int np, int numDataTypes) {
  int numPerPoint;

  numPerPoint = 1 + numDataTypes * (3 + numYears) + np;
  // for each point, we write ltot plus, for each data type, sigma, mean error, days error, yearly sums, plus each param. value

  fwrite(&numPerPoint, sizeof(int), 1, histFile); // write numPerPoint to file
}

// given a double argument, write it as a float to the given binary file
// (writing it as a float rather than a double saves space, at the expense of a little precision)
void writeDoubleAsFloat(double num, FILE *outfile) {
  float theFloat;

  theFloat = (float)num;
  fwrite(&theFloat, sizeof(float), 1, outfile); // write to file
}

// write one point to the .hist file for the given location (loc), in binary format
void writeHistFileBin(FILE *histFile, double ltotnew, double *sigma, OutputInfo *outputInfo, int numDataTypes, SpatialParams *spatialParams, int loc) {
  int i, j;
  int np; // number of changeable parameters
 
  writeDoubleAsFloat(ltotnew, histFile);
  for (i = 0; i < numDataTypes; i++) {
    writeDoubleAsFloat(sigma[i], histFile);
    writeDoubleAsFloat(outputInfo[i].meanError, histFile);
    writeDoubleAsFloat(outputInfo[i].daysError, histFile);
    for (j = 0; j < outputInfo[i].numYears; j++)
      writeDoubleAsFloat(outputInfo[i].years[j], histFile);
  }

  np = spatialParams->numChangeableParams;
  for (i = 0; i < np; i++)
    writeDoubleAsFloat(getSpatialParam(spatialParams, spatialParams->changeableParamIndices[i], loc), histFile);
}

/************************************************************************/

// reset parameter, log likelihood variables, etc. for start of MCMC chain
// parameters from spatialParams through outputInfo are outputs, rest are inputs (spatialParams also provides some inputs)
// all output parameters are arrays except ltotnew, ltotold and ltotmax; 
// loglikely, outputInfo and sigma are spatial (1st dimension of sigma and outputInfo is spatial dimension; loglikely is only one-dimensional)
// if loc = -1, we're running everywhere (number of locations, which is also the size of loglikely and sigma, specified in spatialParams)
// if loc >= 0, we're running at a single location; in this case, 1st dimension of loglikely and sigma only has length 1
// randomStart is boolean: do we start each chain with a random param. set (as opposed to guess values)?
void reset(SpatialParams *spatialParams, double *loglikely, double *ltotnew, double *ltotold, double *ltotmax, 
	   double **sigma, OutputInfo **outputInfo,
	   int loc, int randomStart, double addPercent, FILE *userOut,
	   double (*likely)(double *, OutputInfo *,
			    int, SpatialParams *, double,
			    void (*)(double **, int, int *, SpatialParams *, int),
			    int [], int),
	   double paramWeight, void (*model)(double **, int, int *, SpatialParams *, int),
	   int dataTypeIndices[], int numDataTypes)
{
  int currLoc;
  int np; // number of changeable parameters
  int firstLoc, lastLoc, locIndex;

  np = spatialParams->numChangeableParams;
  if (loc == -1) { // running at all locations
    firstLoc = 0;
    lastLoc = spatialParams->numLocs - 1;
  }
  else // only running at one location
    firstLoc = lastLoc = loc;

  resetSpatialParams(spatialParams, addPercent, randomStart); // reset value, best & knob (set knob to addPercent)

  /*compute log-likelihood of parameter set*/
  for (currLoc = firstLoc; currLoc <= lastLoc; currLoc++) {
    locIndex = currLoc - firstLoc; // index into arrays
    loglikely[locIndex] = -1.0 * (*likely)(sigma[locIndex], outputInfo[locIndex], currLoc, spatialParams, 
					   paramWeight, model, dataTypeIndices, numDataTypes);
  }
  *ltotold = *ltotmax = *ltotnew = sumArray(loglikely, lastLoc - firstLoc + 1);

  /*screen output*/
  fprintf(userOut, "\n\t\t\t**ESTIMATOR**\n");
  writeChangeableParamInfo(spatialParams, loc, userOut);
  fprintf(userOut, "\n\t\tlTOT\tnew= %9.6f\tmax= %9.6f\n",
	  *ltotnew,*ltotmax);
}

/************************************************************************/

/* write (to chainInfo file) all information needed to restart a chain from a given point
   format of file: binary file as follows:
   ltotold ltotmax  <np> * (value best knob)
   where np is the number of changeable parameters
   if loc = -1, then for each spatial parameter we write all values (i.e. value at each point) followed by all bests and all knobs
   if loc >= 0, then we only write value, best and knob at the given location
   where all values are doubles
*/
void writeChainInfo(char *chainInfo, double ltotold, double ltotmax, SpatialParams *spatialParams, int loc) {
  FILE *out;
  int np; // number of changeable parameters
  int i, index;
  int firstLoc, lastLoc, thisFirstLoc, thisLastLoc, currLoc;
  double value[1]; // next value to be written

  out = openFile(chainInfo, "w");

  np = spatialParams->numChangeableParams;
  if (loc == -1) { // running at all locations
    firstLoc = 0;
    lastLoc = spatialParams->numLocs - 1;
  }
  else // only running at one location
    firstLoc = lastLoc = loc;

  // write last and maximum log likelihoods:
  fwrite(&ltotold, sizeof(double), 1, out);
  fwrite(&ltotmax, sizeof(double), 1, out);

  // write parameter info:
  for (i = 0; i < np; i++) {
    index = spatialParams->changeableParamIndices[i]; // get index of ith changeable parameter
    if (isSpatial(spatialParams, index)) {
      thisFirstLoc = firstLoc;
      thisLastLoc = lastLoc;
    }
    else // non-spatial - we'll just use one location (doesn't matter which one)
      thisFirstLoc = thisLastLoc = 0;

    for (currLoc = thisFirstLoc; currLoc <= thisLastLoc; currLoc++) { // if non-spatial, or loc != -1, we'll only execute this loop once     
      value[0] = getSpatialParam(spatialParams, index, currLoc); // fwrite needs a pointer
      fwrite(value, sizeof(double), 1, out);
    }
    for (currLoc = thisFirstLoc; currLoc <= thisLastLoc; currLoc++) { // if non-spatial, or loc != -1, we'll only execute this loop once     
      value[0] = getSpatialParamBest(spatialParams, index, currLoc); // fwrite needs a pointer
      fwrite(value, sizeof(double), 1, out);
    }
    for (currLoc = thisFirstLoc; currLoc <= thisLastLoc; currLoc++) { // if non-spatial, or loc != -1, we'll only execute this loop once     
      value[0] = getSpatialParamKnob(spatialParams, index, currLoc); // fwrite needs a pointer
      fwrite(value, sizeof(double), 1, out);
    }
  }

  fclose(out);
}

/************************************************************************/

// read (from chainInfo file) all information needed to restart a chain from a given point
// (see writeChainInfo function for format of file)
// ltotold and ltotmax are pointers (NOT vectors)
void readChainInfo(char *chainInfo, double *ltotold, double *ltotmax, SpatialParams *spatialParams, int loc) {
  FILE *in;
  int np; // number of changeable parameters
  int i, index;
  int firstLoc, lastLoc, thisFirstLoc, thisLastLoc, currLoc;
  double value[1]; // for the current value we just read
  
  in = openFile(chainInfo, "r");

  np = spatialParams->numChangeableParams;
  if (loc == -1) { // running at all locations
    firstLoc = 0;
    lastLoc = spatialParams->numLocs - 1;
  }
  else // only running at one location
    firstLoc = lastLoc = loc;
  
  // read last and maximum log likelihoods:
  fread(ltotold, sizeof(double), 1, in);
  fread(ltotmax, sizeof(double), 1, in);

  // read parameter info:
  for (i = 0; i < np; i++) {
    index = spatialParams->changeableParamIndices[i]; // get index of ith changeable parameter    
    if (isSpatial(spatialParams, index)) {
      thisFirstLoc = firstLoc;
      thisLastLoc = lastLoc;
    }
    else // non-spatial - we just use one location (doesn't matter which one)
      thisFirstLoc = thisLastLoc = 0;

    for (currLoc = thisFirstLoc; currLoc <= thisLastLoc; currLoc++) { // if non-spatial, or loc != -1, we'll only execute this loop once
      fread(value, sizeof(double), 1, in); // fread needs a pointer
      setSpatialParam(spatialParams, index, currLoc, value[0]);
    }
    for (currLoc = thisFirstLoc; currLoc <= thisLastLoc; currLoc++) { // if non-spatial, or loc != -1, we'll only execute this loop once
      fread(value, sizeof(double), 1, in); // fread needs a pointer
      setSpatialParamBest(spatialParams, index, currLoc, value[0]);
    }
    for (currLoc = thisFirstLoc; currLoc <= thisLastLoc; currLoc++) { // if non-spatial, or loc != -1, we'll only execute this loop once
      fread(value, sizeof(double), 1, in); // fread needs a pointer
      setSpatialParamKnob(spatialParams, index, currLoc, value[0]);
    }
  }

  fclose(in);
}



/************************************************************************/


/* Puts best parameters found in spatialParams
   If loc = -1, run at all locations; if loc >= 0, run only at that single location
   randomStart is boolean: do we start each chain with a random param. set (as opposed to guess values)?
   NOTE: anything but a scale factor of 1 goes against theory */
void metropolis(char *outNameBase, SpatialParams *spatialParams, int loc,
		double (*likely)(double *, OutputInfo *,
				 int, SpatialParams *, double, 
				 void (*)(double **, int, int *, SpatialParams *, int),
				 int [], int),
		void (*model)(double **, int, int *, SpatialParams *, int),
		double addPercent,
		long estSteps, int numAtOnce, int numChains, int randomStart, long numSpinUps, double paramWeight, 
		double scaleFactor,
		int dataTypeIndices[], int numDataTypes,
		FILE *userOut)
{
  const double INC = pow(DEC, ((A_STAR - 1)/A_STAR));
  // want INC^A_STAR * DEC^(1 - A_STAR) = 1

  char histFileBase[256], histFileName[256], chainInfo[256];
  FILE **histFiles; // vector of FILE ptrs (one for each location)
  int accept, yes=0;
  long k=1;
  int chainNum; // which starting chain are we on (for running multiple chains to convergence then choosing best)
  int firstLoc, lastLoc, numLocs; // first, last and number of locations that we're actually running at (based on both loc and spatialParams->numLocs)
  int thisFirstLoc, thisLastLoc; // for spatial params, same as firstLoc and lastLoc; for non-spatial params, both are 0
  int currLoc, locIndex;
  int ichg; // parameter to change
  double range; // (max - min) of current parameter
  double *loglikely; // spatial
  double ltotnew, ltotold,ltotmax;
  double bestChainLtotmax; // ltotmax of the best starting chain so far
  double oldVal;
  double *pold; // spatial
  double pdelta, padd;
  double **sigma; // sigma of this point, estimated by likely function (2-d array: sigma[i][j] is sigma at loc. i, data type j)
  OutputInfo **outputInfo; /* store error info and annual totals for each run & each data type
			      2-d array: outputInfo[i][j] is outputInfo at location i, data type j */
  int converged = 0; /* have we converged on correct param ranges yet?
			we've converged when we're near A_STAR acceptance 
			If numChains > 1, converged will only be set to 1 after we've converged (numChains) times
		     */
  long totalIters = numSpinUps + estSteps; // how many total steps to take once temperatures have converged

  if (loc == -1) { // running at all locations
    numLocs = spatialParams->numLocs;
    firstLoc = 0;
    lastLoc = numLocs - 1;
  }
  else { // only running at one location
    firstLoc = lastLoc = loc;
    numLocs = 1;
  }

  loglikely = makeArray(numLocs);
  pold = makeArray(numLocs);
  sigma = make2DArray(numLocs, numDataTypes);
  outputInfo = (OutputInfo **)malloc(numLocs * sizeof(OutputInfo *));
  for (currLoc = firstLoc; currLoc <= lastLoc; currLoc++)
    outputInfo[currLoc - firstLoc] = newOutputInfo(numDataTypes, currLoc);
  histFiles = (FILE **)malloc(numLocs * sizeof(FILE *)); // one file for each location

  strcpy(histFileBase, outNameBase);
  strcpy(chainInfo, outNameBase);
  strcat(histFileBase, ".hist");
  strcat(chainInfo, ".chain_info");

  chainNum = 1;
  fprintf(userOut, "\n\nRESETTING FOR START CHAIN %d of %d\n\n", chainNum, numChains);
  reset(spatialParams, loglikely, &ltotnew, &ltotold, &ltotmax, sigma, outputInfo,
	loc, randomStart, addPercent, userOut, likely, paramWeight, model, dataTypeIndices, numDataTypes);
  
  writeChangeableParamInfo(spatialParams, loc, userOut);

  bestChainLtotmax = -1.0 * DBL_MAX; // really friggin' small (i.e. really friggin' big in the negative direction)
  k = 1;

  // open all hist files (one for each location), assign file pointers (histFiles), write header to each
  for (currLoc = firstLoc; currLoc <= lastLoc; currLoc++) {
    locIndex = currLoc - firstLoc; // index into array
    sprintf(histFileName, "%s%d", histFileBase, currLoc); // append currLoc to end of histFileBase to get name of current file
    histFiles[locIndex] = openFile(histFileName, "w");

    // writeHistFileHeader(histFiles[locIndex], outputInfo[locIndex][0].numYears, spatialParams->numChangeableParams, numDataTypes);
    writeHistFileBinHeader(histFiles[locIndex], outputInfo[locIndex][0].numYears, spatialParams->numChangeableParams, numDataTypes);    
    /* NOTE: 1) this has to be done AFTER call to reset, since outputInfo.numYears is set in call to likely function
       2) it doesn't matter which data type we use for outputInfo (here we use 0), since all will have the same numYears */
  }

  /*****metropolis loop***********************************************/

  while ((converged == 0) || (k <= totalIters)) {
    accept = 1; // so far, we're in accept mode - we haven't rejected the new point yet

    /*select parameter to change at random*/
    ichg = randomChangeableSpatialParam(spatialParams);

    /* determine first and last location for THIS parameter (depends on whether parameter is spatial)
       note that we'll still use firstLoc and lastLoc for things like actually running model;
       thisFirstLoc and thisLastLoc just refer to locations we care about for parameter-related things like choosing a new parameter value */
    if (isSpatial(spatialParams, ichg)) {
      thisFirstLoc = firstLoc;
      thisLastLoc = lastLoc;
    }
    else // non-spatial - we just use one location (doesn't matter which one)
      thisFirstLoc = thisLastLoc = 0;

    /*change parameter*/
    range = (getSpatialParamMax(spatialParams, ichg) - getSpatialParamMin(spatialParams, ichg)); // range is the same for all locations
    for (currLoc = thisFirstLoc; currLoc <= thisLastLoc; currLoc++) {
      oldVal = getSpatialParam(spatialParams, ichg, currLoc);
      pold[currLoc - thisFirstLoc] = oldVal; // remember old value
      if (accept == 1) { // we haven't set accept to 0 yet (if accept = 0 already, don't bother setting new param. values)
	pdelta = getSpatialParamKnob(spatialParams, ichg, currLoc); // pdelta is expressed as fraction of parameter's range
	padd = (random() * 1.0/RAND_MAX - 0.5) * pdelta * range; 
	// new value will be oldVal + padd
	if (checkSpatialParam(spatialParams, ichg, oldVal + padd) == 0) { // outside allowable range
	  accept = 0; /* if new value is outside allowable range at any location, reject point (equivalent to making likelihood tiny)
			 however, we'll continue the loop, because we need to fill pold vector in order to reset param. values later */
	}
	else
	  setSpatialParam(spatialParams, ichg, currLoc, oldVal + padd); // set new value equal to oldVal + padd
      } // if accept == 1
    } // for currLoc
    
    if (accept == 1) { // we're within allowable range at all locations; run model at all locations and check new total likelihood

      /*compute log-likelihood of new parameter set*/
      for (currLoc = firstLoc; currLoc <= lastLoc; currLoc++) {
	locIndex = currLoc - firstLoc; // index into arrays
	loglikely[locIndex] = -1.0 * (*likely)(sigma[locIndex], outputInfo[locIndex], currLoc, spatialParams, 
					       paramWeight, model, dataTypeIndices, numDataTypes);
      }
      ltotnew = sumArray(loglikely, numLocs);

      // compare "new" likelihood to "max" likelihood and act
      if (ltotnew > ltotmax) {
	ltotmax = ltotnew;
	setAllSpatialParamBests(spatialParams, loc); /* set best equal to current value for all parameters
							if loc == -1, this will set bests at all locations */
      }    

      /*compare new to old and accept or reject*/
      if (ltotnew > ltotold)
	accept = 1;
      else if (randm() < scaleFactor * (ltotnew-ltotold)) // note: anything but a scaleFactor of 1 goes against theory
	accept = 1; 
      else
	accept = 0;
    } // end if (accept == 1)
    
    /* act on acceptance */
    if (accept == 1) {
      /* update likelihoods */
      ltotold = ltotnew;

      yes++; // chalk up one more acceptance

      if (converged == 0) { // we haven't converged yet - twist knob (equivalent to old pdelta)
	for (currLoc = thisFirstLoc; currLoc <= thisLastLoc; currLoc++) {
	  pdelta = getSpatialParamKnob(spatialParams, ichg, currLoc);
	  pdelta = pdelta * INC; // increase temperature
	  // we used to prevent temperature from going above 1, but we no longer care about how big pdelta gets
	  setSpatialParamKnob(spatialParams, ichg, currLoc, pdelta);
	}
      }      
      else if (k > numSpinUps) { // only write to history files if we have been converged for > numSpinUps steps
	/* NOTE: I THINK WE SHOULD TECHNICALLY BE WRITING TO HIST FILE WHETHER WE ACCEPT OR REJECT
	   IF WE REJECT, SHOULD RE-WRITE OLD POINT TO HIST FILE (WILL HAVE TO SAVE OLD LOGLIKELY, SIGMA, AND OUTPUTINFO)
	   TO DO THIS, REMOVE THIS ELSE IF BLOCK, AND PUT IT IN AN IF (CONVERGED && K > NUMSPINUPS) BLOCK
	   AFTER END OF ELSE (REJECTION) BLOCK */
	for (currLoc = firstLoc; currLoc <= lastLoc; currLoc++) {
	  locIndex = currLoc - firstLoc;
	  // writeHistFile(histFiles[locIndex], loglikely[locIndex], sigma[locIndex], outputInfo[locIndex], numDataTypes, spatialParams, currLoc);
	  writeHistFileBin(histFiles[locIndex], loglikely[locIndex], sigma[locIndex], outputInfo[locIndex], numDataTypes, spatialParams, currLoc);
	}
      }
    }

    /* act on rejection */
    else {
      /* return to "old" parameters */
      for (currLoc = thisFirstLoc; currLoc <= thisLastLoc; currLoc++) {
	oldVal = pold[currLoc - thisFirstLoc];
	setSpatialParam(spatialParams, ichg, currLoc, oldVal); // restore old value
      }
      
      if (converged == 0) { // we haven't converged yet - twist knob (equivalent to old pdelta)
	for (currLoc = thisFirstLoc; currLoc <= thisLastLoc; currLoc++) {
	  pdelta = getSpatialParamKnob(spatialParams, ichg, currLoc);
	  pdelta = pdelta * DEC; // decrease temperature
	  if (pdelta < DBL_EPSILON) // don't let temperature get too small 
	    pdelta = DBL_EPSILON;
	  setSpatialParamKnob(spatialParams, ichg, currLoc, pdelta);
	} // for (currLoc)
      } // if (converged == 0)
    } // else (rejection)


    if (k % numAtOnce == 0) { 
      // we've run for numAtOnce iterations - time to output
      
      
      /**************screen output**********************/
      fprintf(userOut, "\n\t\t\tITERATION %6ld\n",k);
      fprintf(userOut, "\t\t\tFRACTION ACCEPTED %3.2f\n\n",yes*1.0/numAtOnce);
      writeChangeableParamInfo(spatialParams, loc, userOut);      
      fprintf(userOut, "\n\t\tlTOT\tnew= %9.6f\tmax= %9.6f\n", ltotnew,ltotmax);
      
      /* END SCREEN OUTPUT */
      
      
      if (converged == 0) { // we haven't converged yet - check for convergence:
	if (fabs(yes*1.0/numAtOnce - A_STAR) < THRESH) { // we've newly-converged
	  if (ltotmax >= bestChainLtotmax) { // this is the best chain we've found so far
	    bestChainLtotmax = ltotmax;
	    // write current info to file so we can restore it later:
	    writeChainInfo(chainInfo, ltotold, ltotmax, spatialParams, loc);
	    fprintf(userOut, "\n\nBEST START CHAIN FOUND SO FAR: WRITING CHAIN INFO TO FILE\n\n");
	  }
	  
	  if (chainNum < numChains) { // reset for the next chain
	    chainNum++; // move on to the next convergence chain
	    fprintf(userOut, "\n\nRESETTING FOR START CHAIN %d of %d\n\n", chainNum, numChains);
	    reset(spatialParams, loglikely, &ltotnew, &ltotold, &ltotmax, sigma, outputInfo, 
		  loc, randomStart, addPercent, userOut, likely, paramWeight, model, dataTypeIndices, numDataTypes);
	  }
	  
	  else { // chainNum >= numChains: we're done running start chains, time to read best from file
	    converged = 1;
	    fprintf(userOut, "\n\nCONVERGED\n\n");
	    
	    if (ltotmax < bestChainLtotmax) { // the most recent chain wasn't the best: read best from file
	      readChainInfo(chainInfo, &ltotold, &ltotmax, spatialParams, loc);	      
	      fprintf(userOut, "\n\nREADING BEST CHAIN INFO FROM FILE:\n\n");
	      writeChangeableParamInfo(spatialParams, loc, userOut);
	      fprintf(userOut, "\n\t\tlTOT\told= %9.6f\tmax= %9.6f\n", ltotold,ltotmax);
	    }
	    
	    else // the most recent chain was the best: no need to read from file
	      fprintf(userOut, "\n\nKEEPING MOST RECENT START CHAIN INFO\n\n");
	    
	  } // end else done running start chains
	  
	  k = 0; // start count over so we run for another estSteps steps
	  
	  /* NOTE: could do a couple tests here:
	     1) halve all param. temperatures (i.e. knobs) after convergence
	     2) set current point to be best point after convergence
	  */
	  
	} // end if we've newly-converged
      } // end if (converged == 0)
      
      yes = 0;
      
    } // end if (k % numAtOnce == 0)
    
    k++;
  } // end metropolis ESTSTEP loop
  
  /* NOTE: may want to print (to file) some measure of best point here
     (e.g. ltotmax; may even want to print outputInfo of best point, which would be more easily done in metropolis loop,
     re-writing best file each time we find a new best point)
  */
  
  // close files, free dynamically-allocated pointers:
  for (currLoc = firstLoc; currLoc <= lastLoc; currLoc++)
    fclose(histFiles[currLoc - firstLoc]);
  free(histFiles);
  free(pold);
  free(loglikely);
  free2DArray((void **)sigma);
  for (currLoc = firstLoc; currLoc <= lastLoc; currLoc++)
    freeOutputInfo(outputInfo[currLoc - firstLoc], numDataTypes);
  free(outputInfo);
}

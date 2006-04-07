
#ifndef ML_METRO_H
#define ML_METRO_H

#include "paramchange.h"
#include "spatialParams.h"


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
		FILE *userOut);

#endif

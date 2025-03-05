// header file for runmean.c
// includes definition of MeanTracker structure that is central to the computation of a running mean

#ifndef RUNMEAN_H
#define RUNMEAN_H

typedef struct MeanTrackerStruct {
  double *values;  // parallel arrays of values
  double *weights; // and weights

  // these will remain constant for any given MeanTracker:
  int length; // length of arrays (i.e. capacity)
  double totWeight; // at any time, sum of all weights of stored values = totWeight

  int start; // index of first (i.e. oldest) value
  int last; // index of most recently-inserted value
  double sum; // current weighted sum of all values stored in array
  // we store this so it is easy to re-compute the sum when we insert a new value
} MeanTracker;


/* PRE: totWeight > 0, maxEntries >= 1 
   allocate space for and return a pointer to a new MeanTracker
   New MeanTracker is initialized to have mean = initMean and appropriate total weight
   Total weight will remain fixed once the MeanTracker is created
   maxEntries gives the maximum number of individual values that can be stored at once
*/
MeanTracker *newMeanTracker(double initMean, double totWeight, int maxEntries);


/* PRE: tracker has been created using newMeanTracker
        so has malloced values and weights arrays, and has length and totWeight set
   reset tracker to have a single entry, with mean initMean 
*/
void resetMeanTracker(MeanTracker *tracker, double initMean);


/* PRE: weight > 0
   Add current value as most recent value in tracker

   Return value:
   0 if all okay
   -1 if error in inputs
   -2 if error because array is too short
*/
int addValueToMeanTracker(MeanTracker *tracker, double value, double weight);


// return the mean of values stored in tracker
double getMeanTrackerMean(MeanTracker *tracker);


// PRE: tracker has been created using newMeanTracker
// free all memory associated with tracker
void deallocateMeanTracker(MeanTracker *tracker);

#endif

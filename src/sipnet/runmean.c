/* runmean: structure and functions to keep track of a running weighted mean

   Each value has an associated weight, and mean is computed over a given total
   weight

   Author: Bill Sacks

   Creation date: 3/4/04
*/

#include <stdlib.h>
#include "runmean.h"
// includes definition of MeanTracker structure

/* PRE: totWeight > 0, maxEntries >= 1
   allocate space for and return a pointer to a new MeanTracker
   New MeanTracker is initialized to have mean = initMean and appropriate total
   weight Total weight will remain fixed once the MeanTracker is created
   maxEntries gives the maximum number of individual values that can be stored
   at once
*/
MeanTracker *newMeanTracker(double initMean, double totWeight, int maxEntries) {
  MeanTracker *tracker;

  // error check:
  if (totWeight <= 0 || maxEntries < 1) {
    return NULL;
  }

  tracker = (MeanTracker *)malloc(sizeof(MeanTracker));
  tracker->values = (double *)malloc(maxEntries * sizeof(double));
  tracker->weights = (double *)malloc(maxEntries * sizeof(double));
  tracker->length = maxEntries;
  tracker->totWeight = totWeight;
  resetMeanTracker(tracker, initMean);

  return tracker;
}

/* PRE: tracker has been created using newMeanTracker
        so has malloced values and weights arrays, and has length and totWeight
   set reset tracker to have a single entry, with mean initMean
*/
void resetMeanTracker(MeanTracker *tracker, double initMean) {
  // start with only one entry, with appropriate mean and total weight
  // don't bother resetting other entries - we'll set them as we come to them
  tracker->start = tracker->last = 0;
  tracker->values[tracker->start] = initMean;
  tracker->weights[tracker->start] = tracker->totWeight;
  tracker->sum = initMean * tracker->totWeight;
}

/* PRE: weight > 0
   Add current value as most recent value in tracker

   Return value:
   0 if all okay
   -1 if error in inputs
   -2 if error because array is too short
*/
int addValueToMeanTracker(MeanTracker *tracker, double value, double weight) {
  double weightLeft;
  int i;

  if (weight <= 0) {  // error
    return -1;  // don't change tracker at all
  } else if (weight >= tracker->totWeight) {  // current value knocks out all
                                              // previous values
    resetMeanTracker(tracker, value);
  } else {  // 0 < weight < totWeight: remove oldest values to make room for new
            // (keeping total weight the same)
    weightLeft = weight;  // how much more do we have to subtract to make room?
    i = tracker->start;  // current position

    while (weightLeft > 0) {  // while we still have to subtract more
      if (tracker->weights[i] > weightLeft) {  // subtract a portion of this one
                                               // and we'll be done
        tracker->weights[i] -= weightLeft;  // subtract correct portion from
                                            // current weight
        tracker->sum -= weightLeft * tracker->values[i];  // subtract correct
                                                          // portion from sum
        weightLeft = 0;  // we're done
      } else {  // remove this value entirely to make room
        tracker->sum -= tracker->weights[i] * tracker->values[i];  // update sum
        weightLeft -= tracker->weights[i];  // we now have this much less weight
                                            // to subtract
        i = (i + 1) % tracker->length;  // forget this value
        // note: we don't have to worry about going past last value, since 1) we
        // check for weight >= tracker->totWeight, and 2) before starting this
        // function, we know that sum(tracker->weights[start..last] ==
        // tracker->totWeight)
      }
    }  // end while: now weightLeft = 0

    tracker->start = i;  // move start position along to correct new position
    i = (tracker->last + 1) % tracker->length;  // point to position of new
                                                // value
    if (i == tracker->start) {  // uh-oh: we've wrapped around and are out of
                                // space!
      // restore previous state and return error value
      // note: this will only happen if we only executed the while loop once,
      // and entered the first if block we can take advantage of this fact to
      // restore the old state
      tracker->weights[i] += weight;  // restore old weight here
      tracker->sum += weight * tracker->values[i];  // restore old sum
      return -2;  // error
    } else {  // there's enough space for the new value
      tracker->last = i;  // increment tracker->last to point to new value
      tracker->values[i] = value;
      tracker->weights[i] = weight;
      tracker->sum += value * weight;  // update sum appropriately
    }
  }  // end else (0 < weight < totWeight)

  return 0;  // all okay
}

// return the mean of values stored in tracker
double getMeanTrackerMean(MeanTracker *tracker) {
  return tracker->sum / tracker->totWeight;
}

// PRE: tracker has been created using newMeanTracker
// free all memory associated with tracker
void deallocateMeanTracker(MeanTracker *tracker) {
  free(tracker->values);
  free(tracker->weights);

  free(tracker);
}

#include "model_utils.h"

#include "common/logging.h"
#include "common/util.h"
#include "state.h"

void ensureNonNegative(double *var, double minVal, const char *label) {
  if (*var < minVal) {
    if (*var < -EPS) {  // Don't print the zeros-except-for-roundoff
      logWarning("Non-negative stock constraint applied for %s (value %g set "
                 "to zero) year %d day %d time %6.3f\n",
                 label, *var, climate->year, climate->day, climate->time);
    }
    *var = 0.;
  }
}

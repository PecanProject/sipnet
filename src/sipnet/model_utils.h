#ifndef MODEL_UTILS_H
#define MODEL_UTILS_H

// Utilities for SIPNET that depend on the model
//
// Model-agnostic utilities should go in common/utils.h

/**
 * Check that input var is not less than input threshold minVal
 *
 * Checks that var >= minVal; if not, set var to zero and issue a warning.
 *
 * Note that if minVal = 0, then this will (as suggested) ensure that var >= 0
 * If minVal > 0, then minVal can be thought of as some epsilon value, below
 *
 * @param var double-valued variable to check
 * @param minVal
 * @param label
 */
void ensureNonNegative(double *var, double minVal, const char *label);

#endif  // MODEL_UTILS_H

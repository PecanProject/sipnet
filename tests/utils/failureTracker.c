// This file is intended to be directly included in test files
#include <math.h>
#include "common/logging.h"

// USAGE:
// -- main() function --
// * call initFailureTracking() near beginning of function
// * call checkStatus() at end
// -- each test case --
// * call checkpointFailureTracking() at beginning
// * call checkFailures() at end
// * use the near() function to compare against expected values
//
// This serves these purposes:
// * Tracks total number of failures
// * Creates consistency between tests
// * Allows easier debugging if there are failures

struct FailureTracker {
  int failures;
  int prevFailures;
  int debug;
};

static struct FailureTracker failureTracker;

void near(double actual, double expected, const char *name) {
  if (!isfinite(actual) || fabs(actual - expected) > 1e-8) {
    logTest("[TEST POINT FAIL] %s: %.15g, expected %.15g\n", name, actual,
            expected);
    failureTracker.failures++;
  }
}

// Call initFailureTracking with a nonzero debug flag to turn on debug mode
void initFailureTracking(int debugFlag) {
  failureTracker.failures = 0;
  failureTracker.prevFailures = 0;
  failureTracker.debug = debugFlag;
}

void checkpointFailureTracking(void) {
  failureTracker.prevFailures = failureTracker.failures;
}

// If debug is on and there is a failure, this will pause the test
void checkFailures(void) {
  if (failureTracker.failures > failureTracker.prevFailures &&
      failureTracker.debug) {
    logTest("FAIL above\n");
    getchar();
  }
}

int getFailureCount(void) { return failureTracker.failures; }

void checkStatus(const char *testName) {
  int numFailures = getFailureCount();
  logTest("\n");
  logTest("%s total failures: %d\n", testName, numFailures);
  logTest("\n");

  if (numFailures > 0) {
    logTest("FAILED %s with status %d\n", testName, 1);
    exit(1);
  }

  logTest("PASSED %s\n", testName);
}

void incrFailureCount(void) { failureTracker.failures++; }

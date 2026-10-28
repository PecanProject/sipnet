#include <math.h>

#include "utils/tUtils.h"
#include "utils/exitHandler.c"
#include "sipnet/balance.h"
#include "sipnet/state.h"

// Unit tests for the pass/fail decision in checkBalance(). The tracker is set
// up so that the pool delta alone produces the requested residual, with the
// system inputs and outputs balancing each other.

typedef enum { CARBON, NITROGEN } Element;

static ClimateNode testClimate;

static void setResidual(Element element, double residual) {
  initBalanceTracker();
  if (element == CARBON) {
    balanceTracker.preTotalC = 0.0;
    balanceTracker.postTotalC = residual;
    balanceTracker.inputsC = 2.0;
    balanceTracker.outputsC = 2.0;
  } else {
    balanceTracker.preTotalN = 0.0;
    balanceTracker.postTotalN = residual;
    balanceTracker.inputsN = 2.0;
    balanceTracker.outputsN = 2.0;
  }
}

static const char *elementName(Element element) {
  return element == CARBON ? "carbon" : "nitrogen";
}

// Returns 0 if checkBalance() exits with EXIT_CODE_INTERNAL_ERROR
static int expectFatal(Element element, double residual, const char *label) {
  int jmp_rval;
  setResidual(element, residual);

  really_exit = 0;
  should_exit = 1;
  expected_code = EXIT_CODE_INTERNAL_ERROR;
  exit_result = 1;
  jmp_rval = setjmp(jump_env);
  if (!jmp_rval) {
    checkBalance();
  }
  test_assert(jmp_rval == 1);

  if (!exit_result) {
    logTest("FAIL: %s %s residual %.17g did not exit as expected\n",
            elementName(element), label, residual);
    return 1;
  }
  return 0;
}

// Returns 0 if checkBalance() continues and zeroes the residual
static int expectPass(Element element, double residual, const char *label) {
  int jmp_rval;
  setResidual(element, residual);

  really_exit = 0;
  should_exit = 0;
  exit_result = 1;
  jmp_rval = setjmp(jump_env);
  if (!jmp_rval) {
    checkBalance();
  }
  test_assert(jmp_rval == 0);

  double delta =
      element == CARBON ? balanceTracker.deltaC : balanceTracker.deltaN;
  if (!exit_result || delta != 0.0) {
    logTest("FAIL: %s %s residual %.17g should pass (delta %.17g)\n",
            elementName(element), label, residual, delta);
    return 1;
  }
  return 0;
}

static int testElement(Element element) {
  int status = 0;
  double justBelow = nextafter(BALANCE_TOLERANCE, 0.0);

  // Balanced and below-threshold residuals are numerical noise
  status |= expectPass(element, 0.0, "zero");
  status |=
      expectPass(element, 0.5 * BALANCE_TOLERANCE, "half-tolerance positive");
  status |=
      expectPass(element, -0.5 * BALANCE_TOLERANCE, "half-tolerance negative");
  status |= expectPass(element, justBelow, "just-below-tolerance positive");
  status |= expectPass(element, -justBelow, "just-below-tolerance negative");

  // At and above the threshold, in both directions
  status |= expectFatal(element, BALANCE_TOLERANCE, "at-tolerance positive");
  status |= expectFatal(element, -BALANCE_TOLERANCE, "at-tolerance negative");
  status |= expectFatal(element, 1e-3, "large positive");
  status |= expectFatal(element, -1e-3, "large negative");

  // Nonfinite residuals
  status |= expectFatal(element, NAN, "NaN");
  status |= expectFatal(element, INFINITY, "+infinity");
  status |= expectFatal(element, -INFINITY, "-infinity");

  return status;
}

static int testResidualPredicate(void) {
  int status = 0;
  double justBelow = nextafter(BALANCE_TOLERANCE, 0.0);

  status |= balanceResidualFailed(0.0) != 0;
  status |= balanceResidualFailed(-0.0) != 0;
  status |= balanceResidualFailed(justBelow) != 0;
  status |= balanceResidualFailed(-justBelow) != 0;
  status |= balanceResidualFailed(BALANCE_TOLERANCE) != 1;
  status |= balanceResidualFailed(-BALANCE_TOLERANCE) != 1;
  status |= balanceResidualFailed(NAN) != 1;
  status |= balanceResidualFailed(INFINITY) != 1;
  status |= balanceResidualFailed(-INFINITY) != 1;

  if (status) {
    logTest("FAIL: balanceResidualFailed() returned an unexpected result\n");
  }
  return status;
}

// A realistic balanced step: inputs - outputs exactly matches the pool change
static int testBalancedStep(void) {
  int jmp_rval;
  initBalanceTracker();
  balanceTracker.preTotalC = 1000.0;
  balanceTracker.postTotalC = 1006.0;
  balanceTracker.inputsC = 10.0;
  balanceTracker.outputsC = 4.0;
  balanceTracker.preTotalN = 50.0;
  balanceTracker.postTotalN = 50.25;
  balanceTracker.inputsN = 1.75;
  balanceTracker.outputsN = 1.5;

  really_exit = 0;
  should_exit = 0;
  exit_result = 1;
  jmp_rval = setjmp(jump_env);
  if (!jmp_rval) {
    checkBalance();
  }
  test_assert(jmp_rval == 0);

  if (!exit_result) {
    logTest("FAIL: balanced step exited unexpectedly\n");
    return 1;
  }
  return 0;
}

int run(void) {
  int status = 0;

  testClimate.year = 2024;
  testClimate.day = 100;
  testClimate.time = 12.0;
  testClimate.length = 1.0;
  climate = &testClimate;

  status |= testResidualPredicate();
  status |= testElement(CARBON);
  status |= testElement(NITROGEN);
  status |= testBalancedStep();

  climate = NULL;
  return status;
}

int main(void) {
  int status;

  logTest("Starting testBalanceCheck:run()\n");
  status = run();

  really_exit = 1;
  if (status) {
    logTest("FAILED testBalanceCheck with status %d\n", status);
    // The exit() stub calls _exit(), which does not flush stdout
    fflush(stdout);
    exit(status);
  }

  logTest("PASSED testBalanceCheck\n");
}

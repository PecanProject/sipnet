#ifndef BALANCE_H
#define BALANCE_H

// Floating-point precision threshold. Differences smaller than this are
// treated as numerical noise.
#define EPS 1e-8

// Tolerance for the mass balance residual. ensureNonNegativeStocks() may zero
// several stocks in [-EPS, 0) in a single step, and those small adjustments all
// end up in the balance residual, so allow for some accumulation.
#define BALANCE_TOLERANCE (10 * EPS)

typedef struct BalanceTrackerStruct {
  // Mass balance checks:
  //   X_t - X_(t-1) = inputs - outputs + tolerance
  // or, we check that
  //   (pool delta) + (system delta) < tolerance
  // where
  //   pool delta = X_t - X_(t-1)
  //   system delta = outputs - inputs

  // Carbon balance
  double preTotalC;
  double postTotalC;
  double inputsC;
  double outputsC;

  // Nitrogen balance
  double preTotalN;
  double postTotalN;
  double inputsN;
  double outputsN;

  // Checks
  double deltaC;
  double deltaN;
} BalanceTracker;

// Global var
extern BalanceTracker balanceTracker;

void updateBalanceTrackerPreUpdate(void);

void updateBalanceTrackerPostUpdate(void);

void initBalanceTracker(void);

// Returns 1 if a balance residual is nonfinite or has magnitude >=
// BALANCE_TOLERANCE
int balanceResidualFailed(double residual);

// Exits with EXIT_CODE_INTERNAL_ERROR if the carbon or nitrogen balance fails
void checkBalance(void);

#endif  // BALANCE_H

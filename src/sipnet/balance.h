#ifndef BALANCE_H
#define BALANCE_H

// Floating-point precision threshold for balance checks and clamping warnings.
// Differences smaller than this are treated as numerical noise.
#define EPS 1e-8

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
  double finalC;
  double inputsC;
  double outputsC;

  // Nitrogen balance
  double preTotalN;
  double postTotalN;
  double finalN;
  double inputsN;
  double outputsN;

  // Stock clamping adjustments: mass added by clamping negative stocks to zero.
  // Positive values indicate that stocks were clamped (mass was "created" to
  // prevent negative pools).
  double clampedC;
  double clampedN;

  // Checks
  double deltaC;
  double deltaN;
} BalanceTracker;

// Global var
extern BalanceTracker balanceTracker;

void updateBalanceTrackerPreUpdate(void);

void updateBalanceTrackerPostUpdate(void);

void updateBalanceTrackerPostClamp(void);

void initBalanceTracker(void);

void checkBalance(void);

#endif  // BALANCE_H

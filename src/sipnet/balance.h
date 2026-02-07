#ifndef BALANCE_H
#define BALANCE_H

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

void checkBalance(void);

#endif  // BALANCE_H

#include "balance.h"

#include <math.h>

#include "common/context.h"
#include "common/exitCodes.h"
#include "common/logging.h"
#include "state.h"

// Definition of global balance tracker struct
BalanceTracker balanceTracker;

void getMassTotals(double *carbon, double *nitrogen) {
  *carbon = (envi.plantWoodC + envi.plantCAccountingDelta) + envi.plantLeafC +
            envi.fineRootC + envi.coarseRootC + envi.soilC;
  if (ctx.litterPool) {
    *carbon += envi.litterC;
  }

  if (ctx.nitrogenCycle) {
    // Note: this is the one place where we use plantWoodC by itself; it's the
    // reason plantCAccountingDelta was created, so that we can ignore it here.
    *nitrogen =
        envi.plantWoodC / params.woodCN + envi.plantLeafC / params.leafCN +
        envi.fineRootC / params.fineRootCN + envi.coarseRootC / params.woodCN +
        envi.soilOrgN + envi.litterN + envi.minN + envi.plantStorageN;
  } else {
    *nitrogen = 0.0;
  }
}

void updateBalanceTrackerPreUpdate(void) {
  // Set the pre-update pool totals
  getMassTotals(&balanceTracker.preTotalC, &balanceTracker.preTotalN);
}

void updateBalanceTrackerPostUpdate(void) {
  // Set the post-update pool totals
  getMassTotals(&balanceTracker.postTotalC, &balanceTracker.postTotalN);

  // Calculate the system inputs and outputs
  // CARBON
  balanceTracker.inputsC = fluxes.photosynthesis +  // GPP
                           fluxes.eventInputC;  // agro event additions
  balanceTracker.outputsC = fluxes.rVeg + fluxes.rFineRoot +
                            fluxes.rCoarseRoot +  // R_a
                            fluxes.rSoil +  // R_h
                            fluxes.soilMethane +  // methane
                            fluxes.eventOutputC;  // agro event removals
  if (ctx.litterPool) {
    balanceTracker.outputsC += fluxes.rLitter + fluxes.litterMethane;
  }
  // Account for climate length
  balanceTracker.inputsC *= climate->length;
  balanceTracker.outputsC *= climate->length;

  // NITROGEN
  if (ctx.nitrogenCycle) {
    balanceTracker.inputsN = fluxes.nFixation + fluxes.eventInputN;
    balanceTracker.outputsN =
        fluxes.nLeaching + fluxes.nVolatilization + fluxes.eventOutputN;

    // Account for climate length
    balanceTracker.inputsN *= climate->length;
    balanceTracker.outputsN *= climate->length;
  }
}

void initBalanceTracker(void) {
  // Initialize all to zero
  // Pools
  balanceTracker.preTotalC = 0.0;
  balanceTracker.postTotalC = 0.0;
  balanceTracker.preTotalN = 0.0;
  balanceTracker.postTotalN = 0.0;

  // System I/O
  balanceTracker.inputsC = 0.0;
  balanceTracker.outputsC = 0.0;
  balanceTracker.inputsN = 0.0;
  balanceTracker.outputsN = 0.0;

  // Checks
  balanceTracker.deltaC = 0.0;
  balanceTracker.deltaN = 0.0;
}

int balanceResidualFailed(double residual) {
  return !isfinite(residual) || fabs(residual) >= BALANCE_TOLERANCE;
}

// %.17g round-trips a double, so differences near EPS remain visible even
// when pool totals are large
static void logBalanceFailure(const char *element, double residual,
                              double preTotal, double postTotal, double inputs,
                              double outputs) {
  logInternalError("%s balance check failed: residual=%.17g (tolerance %g) at "
                   "year %d day %d time %.17g\n",
                   element, residual, BALANCE_TOLERANCE, climate->year,
                   climate->day, climate->time);
  logInternalError("  %s pools: preTotal=%.17g postTotal=%.17g\n", element,
                   preTotal, postTotal);
  logInternalError("  %s system: inputs=%.17g outputs=%.17g\n", element, inputs,
                   outputs);
}

void checkBalance(void) {
  // CARBON
  // Pool delta
  double poolCDelta = balanceTracker.postTotalC - balanceTracker.preTotalC;
  // System delta
  double systemCDelta = balanceTracker.inputsC - balanceTracker.outputsC;
  balanceTracker.deltaC = poolCDelta - systemCDelta;

  // NITROGEN
  // Pool delta
  double poolNDelta = balanceTracker.postTotalN - balanceTracker.preTotalN;
  // System delta
  double systemNDelta = balanceTracker.outputsN - balanceTracker.inputsN;
  balanceTracker.deltaN = poolNDelta + systemNDelta;

  // Finite residuals within tolerance are numerical noise; zero them to avoid
  // weird negative-zero issues. NaN and infinity fail this test and are kept.
  if (fabs(balanceTracker.deltaC) < BALANCE_TOLERANCE) {
    balanceTracker.deltaC = 0.0;
  }
  if (fabs(balanceTracker.deltaN) < BALANCE_TOLERANCE) {
    balanceTracker.deltaN = 0.0;
  }

  int err = 0;
  if (balanceResidualFailed(balanceTracker.deltaC)) {
    err = 1;
    logBalanceFailure("Carbon", balanceTracker.deltaC, balanceTracker.preTotalC,
                      balanceTracker.postTotalC, balanceTracker.inputsC,
                      balanceTracker.outputsC);
  }
  if (balanceResidualFailed(balanceTracker.deltaN)) {
    err = 1;
    logBalanceFailure("Nitrogen", balanceTracker.deltaN,
                      balanceTracker.preTotalN, balanceTracker.postTotalN,
                      balanceTracker.inputsN, balanceTracker.outputsN);
  }
  if (err) {
    logInternalError("Exiting\n");
    exit(EXIT_CODE_INTERNAL_ERROR);
  }
}

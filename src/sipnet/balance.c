#include "balance.h"

#include <math.h>

#include "common/context.h"
#include "common/exitCodes.h"
#include "common/logging.h"
#include "state.h"

// Definition of global balance tracker struct
BalanceTracker balanceTracker;

void getMassTotals(double *carbon, double *nitrogen) {
  *carbon = (envi.plantWoodC + envi.plantWoodCStorageDelta) + envi.plantLeafC +
            envi.fineRootC + envi.coarseRootC + envi.soilC;
  if (ctx.litterPool) {
    *carbon += envi.litterC;
  }

  if (ctx.nitrogenCycle) {
    // Note: this is the one place where we use plantWoodC by itself; it's the
    // reason plantWoodCStorageDelta was created, so that we can ignore it here.
    *nitrogen =
        envi.plantWoodC / params.woodCN + envi.plantLeafC / params.leafCN +
        envi.fineRootC / params.fineRootCN + envi.coarseRootC / params.woodCN +
        envi.soilOrgN + envi.litterN + envi.minN;
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
}

void updateBalanceTrackerPostClamp(void) {
  getMassTotals(&balanceTracker.finalC, &balanceTracker.finalN);

  // Difference between post-update and post-clamp is the amount we "gained" by
  // setting negative values to zero
  balanceTracker.clampedC = balanceTracker.finalC - balanceTracker.postTotalC;
  if (balanceTracker.clampedC < -EPS) {
    // This shouldn't happen, by construction
    logInternalError("Non-negative clamping has cause carbon loss\n");
  }
  if (balanceTracker.clampedC < EPS) {
    balanceTracker.clampedC = 0;
  }

  balanceTracker.clampedN = balanceTracker.finalN - balanceTracker.postTotalN;
  if (balanceTracker.clampedN < -EPS) {
    // This shouldn't happen, by construction
    logInternalError("Non-negative clamping has cause nitrogen loss\n");
  }
  if (balanceTracker.clampedN < EPS) {
    balanceTracker.clampedN = 0;
  }

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

  // Account for gains from clamping
  balanceTracker.inputsC += balanceTracker.clampedC;
  if (ctx.nitrogenCycle) {
    balanceTracker.inputsN += balanceTracker.clampedN;
  }
}

void initBalanceTracker(void) {
  // Initialize all to zero
  // Pools
  balanceTracker.preTotalC = 0.0;
  balanceTracker.postTotalC = 0.0;
  balanceTracker.clampedC = 0.0;
  balanceTracker.finalC = 0.0;
  balanceTracker.preTotalN = 0.0;
  balanceTracker.postTotalN = 0.0;
  balanceTracker.clampedN = 0.0;
  balanceTracker.finalN = 0.0;

  // System I/O
  balanceTracker.inputsC = 0.0;
  balanceTracker.outputsC = 0.0;
  balanceTracker.inputsN = 0.0;
  balanceTracker.outputsN = 0.0;

  // Checks
  balanceTracker.deltaC = 0.0;
  balanceTracker.deltaN = 0.0;
}

void checkBalance(void) {
  // CARBON
  // Pool delta
  double poolCDelta = balanceTracker.finalC - balanceTracker.preTotalC;
  // System delta
  double systemCDelta = balanceTracker.inputsC - balanceTracker.outputsC;
  balanceTracker.deltaC = poolCDelta - systemCDelta;

  // NITROGEN
  // Pool delta
  double poolNDelta = balanceTracker.finalN - balanceTracker.preTotalN;
  // System delta
  double systemNDelta = balanceTracker.inputsN - balanceTracker.outputsN;
  balanceTracker.deltaN = poolNDelta - systemNDelta;

  // To avoid weird negative-zero issues...
  if (fabs(balanceTracker.deltaC) < EPS) {
    balanceTracker.deltaC = 0.0;
  }
  if (fabs(balanceTracker.deltaN) < EPS) {
    balanceTracker.deltaN = 0.0;
  }

  int err = 0;
  if (fabs(balanceTracker.deltaC) > 0.0) {
    // err = 1;
    //  logInternalError(  someday
    logWarning(
        "Carbon balance check failed (delta=%8.4f, Y: %d D: %d T: %4.2f)\n",
        balanceTracker.deltaC, climate->year, climate->day, climate->time);
  }
  if (fabs(balanceTracker.deltaN) > 0.0) {
    // err = 1;
    // logInternalError(  someday
    logWarning(
        "Nitrogen balance check failed (delta=%8.4f, Y: %d D: %d T: %4.2f)\n",
        balanceTracker.deltaN, climate->year, climate->day, climate->time);
    logWarning("preTot %8.5f postTot %8.5f input %8.5f output %8.5f clamped "
               "%8.5f delta %8.5f\n",
               balanceTracker.preTotalN, balanceTracker.postTotalN,
               balanceTracker.inputsN, balanceTracker.outputsN,
               balanceTracker.clampedN, balanceTracker.deltaN);
  }
  if (err) {
    logInternalError("Exiting\n");
    exit(EXIT_CODE_INTERNAL_ERROR);
  }
}

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

  // Calculate the system inputs and outputs
  // CARBON
  balanceTracker.inputsC = fluxes.photosynthesis +  // GPP
                           fluxes.eventInputC;  // agro event additions
  balanceTracker.outputsC = fluxes.rVeg + fluxes.rFineRoot +
                            fluxes.rCoarseRoot +  // R_a
                            fluxes.rSoil +  // R_h
                            fluxes.eventOutputC;  // agro event removals
  if (ctx.litterPool) {
    balanceTracker.outputsC += fluxes.rLitter;
  }
  // Account for climate length
  balanceTracker.inputsC *= climate->length;
  balanceTracker.outputsC *= climate->length;

  // NITROGEN
  if (ctx.nitrogenCycle) {
    balanceTracker.inputsN =
        // TODO: fluxes.fixation +
        fluxes.eventInputN;
    balanceTracker.outputsN =
        fluxes.nLeaching + fluxes.nVolatilization + fluxes.eventOutputN;

    // Account for climate length
    balanceTracker.inputsN *= climate->length;
    balanceTracker.outputsN *= climate->length;
  }
}

void initBalanceTracker(void) {
  // Initialize all to zero
  balanceTracker.preTotalC = 0.0;
  balanceTracker.postTotalC = 0.0;
  balanceTracker.inputsC = 0.0;
  balanceTracker.outputsC = 0.0;
  balanceTracker.preTotalN = 0.0;
  balanceTracker.postTotalN = 0.0;
  balanceTracker.inputsN = 0.0;
  balanceTracker.outputsN = 0.0;
  balanceTracker.deltaC = 0.0;
  balanceTracker.deltaN = 0.0;
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
  double systemNDelta = balanceTracker.inputsN - balanceTracker.outputsN;
  balanceTracker.deltaN = poolNDelta - systemNDelta;

  // To avoid weird negative-zero issues...
  if (fabs(balanceTracker.deltaC) < 1e-8) {
    balanceTracker.deltaC = 0.0;
  }
  if (fabs(balanceTracker.deltaN) < 1e-8) {
    balanceTracker.deltaN = 0.0;
  }

  int err = 0;
  if (fabs(balanceTracker.deltaC) > 0.0) {
    err = 1;
    logInternalError(
        "Carbon balance check failed (delta=%8.4f, Y: %d D: %d T: %4.2f)\n",
        balanceTracker.deltaC, climate->year, climate->day, climate->time);
  }
  // RE-ENABLE WHEN N IS FIXED
  // if (fabs(balanceTracker.deltaN) > 0.0) {
  //  err = 1;
  //  logInternalError("Nitrogen balance check failed (delta=%8.4f)\n",
  //                   balanceTracker.deltaN);
  //}
  if (err) {
    logInternalError("Exiting\n");
    //  exit(EXIT_CODE_INTERNAL_ERROR);
  }
}

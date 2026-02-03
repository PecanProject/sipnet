#include "balance.h"
#include "common/context.h"
#include "state.h"

// Definition of global balance tracker struct
BalanceTracker balanceTracker;

void getMassTotals(double *carbon, double *nitrogen) {
  *carbon = envi.plantWoodC + envi.plantLeafC + envi.fineRootC +
            envi.coarseRootC + envi.soilC + envi.nppStorage;
  if (ctx.litterPool) {
    *carbon += envi.litterC;
  }

  if (ctx.nitrogenCycle) {
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

  // TBD: warn if balance off?
}

#include "balance.h"

#include <math.h>

#include "common/context.h"
#include "common/exitCodes.h"
#include "common/logging.h"
#include "state.h"

// Definition of global balance tracker struct
BalanceTracker balanceTracker;

// No leaf-water pool is tracked in envi; ctx.leafWater only caps the immedEvap
// flux rate (see calcPrecip()). Excess intercepted water goes to soil via
// netRain.
static double getWaterTotal(void) { return envi.soilWater + envi.snow; }

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
        envi.soilOrgN + envi.litterN + envi.minN + envi.plantStorageN;
  } else {
    *nitrogen = 0.0;
  }
}

void updateBalanceTrackerPreUpdate(void) {
  // Set the pre-update pool totals
  getMassTotals(&balanceTracker.preTotalC, &balanceTracker.preTotalN);
  balanceTracker.preTotalWater = getWaterTotal();
}

void updateBalanceTrackerPostUpdate(void) {
  // Set the post-update pool totals
  getMassTotals(&balanceTracker.postTotalC, &balanceTracker.postTotalN);
  balanceTracker.postTotalWater = getWaterTotal();
}

void updateBalanceTrackerPostClamp(void) {
  getMassTotals(&balanceTracker.finalC, &balanceTracker.finalN);
  balanceTracker.finalWater = getWaterTotal();

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

  balanceTracker.clampedWater =
      balanceTracker.finalWater - balanceTracker.postTotalWater;
  if (balanceTracker.clampedWater < -EPS) {
    // This shouldn't happen, by construction
    logInternalError("Non-negative clamping has cause water loss\n");
  }
  if (balanceTracker.clampedWater < EPS) {
    balanceTracker.clampedWater = 0;
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

  // WATER
  // eventEvap: irrigation water immediately evaporated; counted as both input
  // (water entered the system) and output (left as vapour). Net to soil is
  // eventSoilWater only. snowMelt is excluded (internal snow -> soil transfer).
  balanceTracker.inputsWater =
      fluxes.rain + fluxes.snowFall + fluxes.eventSoilWater + fluxes.eventEvap;
  balanceTracker.outputsWater =
      fluxes.immedEvap + fluxes.transpiration + fluxes.evaporation +
      fluxes.sublimation + fluxes.drainage + fluxes.fastFlow + fluxes.eventEvap;

  // Account for climate length
  balanceTracker.inputsWater *= climate->length;
  balanceTracker.outputsWater *= climate->length;

  // Account for gains from clamping
  balanceTracker.inputsC += balanceTracker.clampedC;
  if (ctx.nitrogenCycle) {
    balanceTracker.inputsN += balanceTracker.clampedN;
  }
  balanceTracker.inputsWater += balanceTracker.clampedWater;
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
  balanceTracker.preTotalWater = 0.0;
  balanceTracker.postTotalWater = 0.0;
  balanceTracker.clampedWater = 0.0;
  balanceTracker.finalWater = 0.0;

  // System I/O
  balanceTracker.inputsC = 0.0;
  balanceTracker.outputsC = 0.0;
  balanceTracker.inputsN = 0.0;
  balanceTracker.outputsN = 0.0;
  balanceTracker.inputsWater = 0.0;
  balanceTracker.outputsWater = 0.0;

  // Checks
  balanceTracker.deltaC = 0.0;
  balanceTracker.deltaN = 0.0;
  balanceTracker.deltaWater = 0.0;
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

  // WATER
  // Pool delta
  double poolWaterDelta =
      balanceTracker.finalWater - balanceTracker.preTotalWater;
  // System delta
  double systemWaterDelta =
      balanceTracker.inputsWater - balanceTracker.outputsWater;
  balanceTracker.deltaWater = poolWaterDelta - systemWaterDelta;

  // To avoid weird negative-zero issues...
  if (fabs(balanceTracker.deltaC) < EPS) {
    balanceTracker.deltaC = 0.0;
  }
  if (fabs(balanceTracker.deltaN) < EPS) {
    balanceTracker.deltaN = 0.0;
  }
  if (fabs(balanceTracker.deltaWater) < EPS) {
    balanceTracker.deltaWater = 0.0;
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
  if (fabs(balanceTracker.deltaWater) > 0.0) {
    logWarning(
        "Water balance check failed (delta=%8.4f, Y: %d D: %d T: %4.2f)\n",
        balanceTracker.deltaWater, climate->year, climate->day, climate->time);
  }
  if (err) {
    logInternalError("Exiting\n");
    exit(EXIT_CODE_INTERNAL_ERROR);
  }
}

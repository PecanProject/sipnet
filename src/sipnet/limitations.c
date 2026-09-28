#include "limitations.h"

#include <math.h>

#include "common/context.h"
#include "common/logging.h"
#include "common/util.h"

#include "nitrogen.h"
#include "state.h"

// See limitations.h
void checkLeafOnLimitation(double *leafOnFlux) {
  // Leaf on events are limited by:
  // * leafGrowth parameter (input leafOn value)
  // * available carbon
  // * available nitrogen
  double leafOnCDemand = *leafOnFlux * climate->length;

  if (leafOnCDemand < TINY) {
    // Nothing to check
    return;
  }

  // First up, carbon. We do not draw from the C storage pool for this.
  double availableC =
      (envi.plantWoodC + envi.coarseRootC) * params.leafOnReallocFrac;
  double cLimiter = availableC / leafOnCDemand;

  double leafOnNDemand = 0.0;
  double nLimiter = 1.0;
  double availableN = 0.0;
  // Next, nitrogen; leaf-on only draws from the plantStorageN pool
  if (ctx.nitrogenCycle) {
    // Needed N for this transfer is (what leaves need) - (what wood provides)
    // Reminder: both wood and coarseRoot use params.woodCN, so no need to
    // treat those C demands separately
    leafOnNDemand = calcLeafOnNFromC(leafOnCDemand);
    availableN = envi.plantStorageN;
    if (leafOnNDemand > TINY) {
      nLimiter = availableN / leafOnNDemand;
    }
  }
  double limitation = unitClip(fmin(cLimiter, nLimiter));

  if (limitation < 1) {
    *leafOnFlux *= limitation;
    if (ctx.nitrogenCycle) {
      logInfo("Leaf on creation %.4f C / %.4f N exceeds available C %.4f / N "
              "%.4f (C ratio: %.4f, N ratio: %.4f), "
              "reducing leaf-on growth by %.2f%% on year %d day %d time %.3f\n",
              leafOnCDemand, leafOnNDemand, availableC, availableN, cLimiter,
              nLimiter, (1 - limitation) * 100, climate->year, climate->day,
              climate->time);
    } else {
      logInfo("Leaf on creation %.4f exceeds available C %.4f "
              "(C ratio: %.4f, N ratio: %.4f), "
              "reducing leaf-on growth by %.2f%% on year %d day %d time %.3f\n",
              leafOnCDemand, availableC, cLimiter, nLimiter,
              (1 - limitation) * 100, climate->year, climate->day,
              climate->time);
    }
  }
}

/**
 * Check that negative growth is not driving a pool to end negative
 *
 * Adjust if necessary
 */
static void checkNegativeCreation(void) {
  // In the case of negative growth (mean npp < 0), we might be allocating that
  // negative growth to a pool that can't handle it (e.g., leaf creation is
  // negative, but leaf pool is already at 0). In those cases, adjust
  // appropriately.

  double len = climate->length;

  logInfo("checkNeg BEFORE: leafLitter %f eventLOLitter %f leafC %f "
          "leafCreation %f "
          "eventLONResorp %f eventLOLitterN %f eventLitterN %f\n",
          fluxes.leafLitter * len, fluxes.eventLeafOffLitterC * len,
          envi.plantLeafC, fluxes.leafCreation * len,
          fluxes.eventLeafOffNResorption * len,
          fluxes.eventLeafOffLitterN * len, fluxes.eventLitterN);

  // Above ground
  // If leafCreation is too negative, we need to deduct from wood instead
  // Need to make sure we don't go too far the other way. Leaf off litter
  // (either fluxes.leafLitter or fluxes.eventLeafOffLitter) might also
  // incorrectly drive the pool negative, sp adjust for that too.

  // First we handle leaf litter. Assuming params.leafTurnoverRate is valid
  // (ie, <=1), availableLeafRate should be non-negative.
  double availableLeafRate = envi.plantLeafC / len - fluxes.leafLitter;

  // Reminder: litter and creation fluxes "point" in the opposite direction, so
  // their signs are opposite here
  double deficit = fluxes.leafOffLitter + fluxes.eventLeafOffLitterC -
                   fluxes.leafCreation - availableLeafRate;
  if (deficit > 0) {
    // If negative growth + leaf off is too much, let's first reduce leaf off
    if (fluxes.eventLeafOffLitterC > 0) {
      double adjust = fmin(fluxes.eventLeafOffLitterC, deficit);
      fluxes.eventLeafOffLitterC -= adjust;
      deficit -= adjust;
    }
    if (deficit > 0) {
      if (fluxes.leafOffLitter > 0) {
        double adjust = fmin(fluxes.leafOffLitter, deficit);
        fluxes.leafOffLitter -= adjust;
        deficit -= adjust;
      }
    }
    // Next, move some negative growth to the wood pool
    if (deficit > 0) {
      // If this is too much for the wood pool to handle, we have an error that
      // will be caught in ensureNonNegative
      fluxes.woodCreation -= deficit;
      fluxes.leafCreation += deficit;
    }
  }

  logInfo("checkNeg AFTER:  leafLitter %f eventLOLitter %f leafC %f "
          "leafCreation %f "
          "eventLONResorp %f eventLOLitterN %f eventLitterN %f\n",
          fluxes.leafLitter * len, fluxes.eventLeafOffLitterC * len,
          envi.plantLeafC, fluxes.leafCreation * len,
          fluxes.eventLeafOffNResorption * len,
          fluxes.eventLeafOffLitterN * len, fluxes.eventLitterN);

  // Below ground
  double fineRootDeficit =
      envi.fineRootC / len + fluxes.fineRootCreation - fluxes.fineRootLoss;
  double coarseRootDeficit = envi.coarseRootC / len +
                             fluxes.coarseRootCreation - fluxes.coarseRootLoss;
  if ((fineRootDeficit < 0.0) != (coarseRootDeficit < 0.0)) {
    // If neither are negative, nothing to do
    // If both are negative, the plant will die in checkForMortality()
    if (fineRootDeficit < 0.0) {
      fluxes.coarseRootCreation += fineRootDeficit;
      fluxes.fineRootCreation -= fineRootDeficit;
    }
    if (coarseRootDeficit < 0.0) {
      fluxes.fineRootCreation += coarseRootDeficit;
      fluxes.coarseRootCreation -= coarseRootDeficit;
    }
  }
}

/**
 * Check for nitrogen limitation, and reduce growth if needed
 */
static void checkNitrogenLimitation(void) {
  double len = climate->length;
  logInfo("checkNLimit BEFORE: leafLitter %f eventLOLitter %f leafC %f "
          "leafCreation %f "
          "eventLONResorp %f eventLOLitterN %f eventLitterN %f\n",
          fluxes.leafLitter * len, fluxes.eventLeafOffLitterC * len,
          envi.plantLeafC, fluxes.leafCreation * len,
          fluxes.eventLeafOffNResorption * len,
          fluxes.eventLeafOffLitterN * len, fluxes.eventLitterN);

  // First, determine if we are in a nitrogen-limited situation. The uptake
  // flux has already taken the storage pool into account, so we just need to
  // see if that uptake is too much, taking into account other fluxes to the
  // minN pool.
  // Calc total delta to minN pool
  // double len = climate->length;
  double uptakeDemand = fluxes.nUptake * len;
  double nonUptakeDelta = calcMinNNonUptakeFluxes() * len;
  double availableMinN = envi.minN + nonUptakeDelta;

  if (uptakeDemand > TINY && uptakeDemand > availableMinN) {
    // More demand than supply - N limitation is in effect
    // Unfortunately, plantStorageN effects make this non-linear

    // Unmet demand
    // Current uptakeDemand is:
    //   upD = (D - S) * u
    // where D is plant demand, S is unclaimed storage, and u is the uptake frac
    // This is currently more than N (available minN). We want reduction factor
    // k such that:
    //   upD = (kD - S) * u = N
    // Solving for k:
    //   k = [N/u + S] / D
    double unclaimedStorage = calcUnclaimedStorageN();
    double demand = calcPlantNDemandFlux() * len;
    double uptakeFrac = 1 - calcNFixationFrac();
    double reduction = (availableMinN / uptakeFrac + unclaimedStorage) / demand;

    logInfo("N limitation: available soil min N %.4f + storage N %.4f < plant N"
            " demand %.4f - N fixation %,4f, "
            "reducing plant growth by %.2f%% on year %d day %d time %.3f\n",
            availableMinN, unclaimedStorage, demand, fluxes.nFixation * len,
            (1 - reduction) * 100, climate->year, climate->day, climate->time);

    // Reduce all drains on soil N (all fluxes used in calcPlantNDemandFlux,
    // plus fixation and uptake)
    fluxes.woodCreation *= reduction;
    fluxes.leafCreation *= reduction;
    fluxes.fineRootCreation *= reduction;
    fluxes.coarseRootCreation *= reduction;

    // If there was a leaf-off event in this step, it is possible that we have
    // reduced leaf growth down to the point where the pool will go negative
    // Re-call that check
    checkNegativeCreation();
    // double leafOffFlux = fluxes.leafOffLitter + fluxes.eventLeafOffLitter;
    // if (leafOffFlux > 0) {
    //   logInfo("Leaf-off flux %.f, leaf pool %.f, leaf creation %.f leaf
    //   litter %.f\n",
    //           leafOffFlux * len, envi.plantLeafC, fluxes.leafCreation * len,
    //           fluxes.leafLitter * len);
    //   double leafPool = envi.plantLeafC +
    //     (fluxes.leafCreation + fluxes.leafLitter + leafOffFlux) * len;
    //   if (leafPool < 0) {
    //     // Reduce leaf-off litter to avoid negative pool; note that only one
    //     of
    //     // these will be > 0, meaning the other is unaffected by this calc
    //     fluxes.leafOffLitter = fmax(0.0, fluxes.leafOffLitter + leafPool);
    //     fluxes.eventLeafOffLitter =
    //       fmax(0.0, fluxes.eventLeafOffLitter + leafPool);
    //   }
    //   // Recalc N effects for event leaf off; will be a no-op if there was
    //   // no event leaf off

    // Reset and recalc event leaf-off N
    fluxes.eventLeafOffNResorption = 0.0;
    fluxes.eventLeafOffLitterN = 0.0;
    calcLeafOffNEffects(fluxes.eventLeafOffLitterC * len,
                        &fluxes.eventLeafOffNResorption,
                        &fluxes.eventLeafOffLitterN);
    // }

    // Reset N calculations
    calcNitrogenFluxes();
  }

  logInfo("checkNLimit AFTER:  leafLitter %f eventLOLitter %f leafC %f "
          "leafCreation %f "
          "eventLONResorp %f eventLOLitterN %f eventLitterN %f\n",
          fluxes.leafLitter * len, fluxes.eventLeafOffLitterC * len,
          envi.plantLeafC, fluxes.leafCreation * len,
          fluxes.eventLeafOffNResorption * len,
          fluxes.eventLeafOffLitterN * len, fluxes.eventLitterN);
}

/**
 * Check if leaching and volatilization will drive mineral N negative
 */
static void checkMineralNLimitation(void) {
  double len = climate->length;
  double pool = envi.minN + (fluxes.nMin + fluxes.eventMinN) * len;
  double loss = (fluxes.nLeaching + fluxes.nVolatilization) * len;

  if (loss > TINY && loss > pool) {
    double reduction = pool / loss;
    fluxes.nLeaching *= reduction;
    fluxes.nVolatilization *= reduction;
  }
}

// See limitations.h
void checkLimitations(void) {
  // Our only post-flux limitation to check
  if (ctx.nitrogenCycle) {
    // Call the mineral N check before the general N Limitation check
    checkMineralNLimitation();
    checkNitrogenLimitation();
  }
}

// See limitations.h
void checkCarbonLimitations(void) { checkNegativeCreation(); }

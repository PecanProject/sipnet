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
 * Check for nitrogen limitation, and reduce growth if needed
 */
static void checkNitrogenLimitation(void) {
  // First, determine if we are in a nitrogen-limited situation
  double maxDemandFlux = calcPlantNDemand();
  double maxDemand = maxDemandFlux * climate->length;

  double availableMinN = calcPlantAvailableN();

  double nFixationFrac = calcNFixationFrac();
  double maxUptake = maxDemand * (1 - nFixationFrac);

  if (maxUptake > TINY && maxUptake > availableMinN) {
    // More demand than supply - N limitation is in effect
    double reduction = availableMinN / maxUptake;
    logInfo("N uptake %.4f exceeds available soil min N %.4f, reducing plant "
            "growth by %.2f%% on year %d day %d time %.3f\n",
            maxUptake, availableMinN, (1 - reduction) * 100, climate->year,
            climate->day, climate->time);

    // Reduce all drains on soil N (all fluxes used in calcPlantNDemand, plus
    // fixation and uptake)
    fluxes.woodCreation *= reduction;
    fluxes.leafCreation *= reduction;
    fluxes.fineRootCreation *= reduction;
    fluxes.coarseRootCreation *= reduction;
    fluxes.nFixation *= reduction;
    fluxes.nUptake *= reduction;
  }
}

// See limitations.h
void checkLimitations(void) {
  // Our only post-flux limitation to check
  if (ctx.nitrogenCycle) {
    checkNitrogenLimitation();
  }
}

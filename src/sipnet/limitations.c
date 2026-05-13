#include "limitations.h"

#include <math.h>

#include "common/context.h"
#include "common/logging.h"
#include "common/util.h"

#include "nitrogen.h"
#include "state.h"

/**
 * Check for carbon and nitrogen limitations for leaf-on events
 */
static void checkLeafOnLimitation(void) {
  // Leaf on events are limited by:
  // * leafGrowth parameter (this is already calculated)
  // * available carbon
  // * available nitrogen
  double leafOnFlux = fluxes.leafOnCreation + fluxes.eventLeafOnCreation;
  double leafOnCDemand = leafOnFlux * climate->length;
  double leafOnNDemand = 0.0;
  double nLimitation = 1.0;
  double availableN = 0.0;

  if (leafOnCDemand < TINY) {
    // Nothing to check
    return;
  }

  // First up, carbon. We do not draw from the storage pool for this.
  double availableC = envi.plantWoodC + envi.coarseRootC;
  double cLimitation = (availableC > TINY) ? (availableC / leafOnCDemand) : 0;

  // Next, nitrogen. This one is a little trickier, as 'available nitrogen' is
  // harder to calculate (but we defer that to the nitrogen module).
  if (ctx.nitrogenCycle) {
    // Needed N for this transfer is (what leaves need) - (what wood provides)
    // Reminder: both wood and coarseRoot use params.woodCN
    leafOnNDemand =
        leafOnCDemand / params.leafCN - leafOnCDemand / params.woodCN;
    double minNFluxes = calcMinNNonUptakeFluxes() + fluxes.eventMinN;
    availableN = envi.minN + minNFluxes * climate->length;
    nLimitation = (availableN > TINY) ? (availableN / leafOnNDemand) : 0;
  }
  double limitation = fmin(cLimitation, nLimitation);
  limitation = fmax(fmin(limitation, 1.0), 0.0);

  if (limitation < 1) {
    fluxes.leafOnCreation *= limitation;
    fluxes.eventLeafOnCreation *= limitation;
    if (ctx.nitrogenCycle) {
      // We need to recalculate fixation and uptake since the demand has now
      // changed
      calcNFixationAndUptakeFluxes();

      logInfo("Leaf on creation %.4f C / %.4f N exceeds available C %.4f / N "
              "%.4f, "
              "reducing leaf on by %.2f%% on year %d day %d time %.3f\n",
              leafOnCDemand, leafOnNDemand, availableC, availableN,
              (1 - limitation) * 100, climate->year, climate->day,
              climate->time);
      logInfo("envi.minN %.3f fluxes.nMin %.3f fluxes.nVol %.3f fluxes.nLeach "
              "%.3f fluxes.nFix %.3f fluxes.nUptake %.3f\n",
              envi.minN, fluxes.nMin * climate->length,
              fluxes.nVolatilization * climate->length,
              fluxes.nLeaching * climate->length,
              fluxes.nFixation * climate->length,
              fluxes.nUptake * climate->length);
    } else {
      logInfo("Leaf on creation %.4f exceeds available C %.4f, "
              "reducing leaf on by %.2f%% on year %d day %d time %.3f\n",
              leafOnCDemand, availableC, (1 - limitation) * 100, climate->year,
              climate->day, climate->time);
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

  double nonUptakeFluxes = calcMinNNonUptakeFluxes();
  double availableMinN =
      fmax(0, envi.minN + (nonUptakeFluxes * climate->length));

  double nFixationFrac = calcNFixationFrac();
  double maxUptake = maxDemand * (1 - nFixationFrac);

  if (maxUptake > TINY && maxUptake > availableMinN) {
    // More demand than supply - N limitation is in effect
    double reduction = availableMinN / maxUptake;
    logInfo("N uptake %.4f exceeds available soil min N %.4f, reducing plant "
            "growth by %.2f%% on year %d day %d time %.3f\n",
            maxUptake, availableMinN, (1 - reduction) * 100, climate->year,
            climate->day, climate->time);

    // Reduce all drains on soil N
    fluxes.eventLeafOnCreation *= reduction;
    fluxes.leafOnCreation *= reduction;
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

  // First up - leaf on. This should be before the nitrogen limitation check,
  // as the two are not independent.
  checkLeafOnLimitation();

  if (ctx.nitrogenCycle) {
    checkNitrogenLimitation();
  }
}

#include "limitations.h"

#include <math.h>

#include "common/context.h"
#include "common/logging.h"
#include "common/util.h"

#include "nitrogen.h"
#include "state.h"

void checkNitrogenLimitation(void) {
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

void checkLimitations() {
  // EXPECTED: calcLeafOnLimitation()

  if (ctx.nitrogenCycle) {
    checkNitrogenLimitation();
  }
}

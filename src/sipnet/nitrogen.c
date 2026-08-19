#include "nitrogen.h"

#include <math.h>

#include "common/context.h"
#include "common/logging.h"
#include "common/util.h"

#include "depeffects.h"
#include "state.h"

/*!
 * Calculate mineral N volatilization flux
 */
static void calcNVolatilizationFlux(void) {
  // flux = k_vol * nMin * Dtemp * Dwater
  // Note k_vol is in units of day^-1, so we do not need to divide
  // by climate length to make this a flux
  double d_temp = calcTempEffect(climate->tsoil);
  double d_water =
      calcVolatilizationMoistEffect(envi.soilWater, params.soilWHC);

  fluxes.nVolatilization =
      params.nVolatilizationFrac * envi.minN * d_temp * d_water;
}

/*!
 * Calculate mineral N leaching flux
 */
static void calcNLeachingFlux(void) {
  double phi;
  // phi is (drainage / soilWHC) between 0 and 1
  if ((fluxes.drainage / params.soilWHC) < 1) {
    phi = fluxes.drainage / params.soilWHC;
  } else {
    phi = 1;
  }
  // flux = nMin * phi * leaching fraction, g N * m^-2 * day^-1
  fluxes.nLeaching = envi.minN * phi * params.nLeachingFrac;
}

/**
 * Calculate nitrogen fluxes for soil and litter pools
 */
static void calcNPoolFluxes(void) {
  // C:N ratios for litter and soil, needed in most of the succeeding calcs
  double litterCN = calcRatio(envi.litterC, envi.litterN);
  double soilCN = calcRatio(envi.soilC, envi.soilOrgN);

  // for both litter and soil, mineralization is calculated as heterotrophic
  // respiration divided by the C:N ratio of that pool.
  double litterMin = fluxes.rLitter / litterCN;
  double soilMin = fluxes.rSoil / soilCN;

  // Adding soil carbon saturation functionality so organic N fluxes to soil
  // and litter are proportional to respective carbon fluxes dependent on
  // soil carbon saturation
  double soilNInputs = fluxes.litterToSoil / litterCN +
                       fluxes.fineRootLoss / params.fineRootCN +
                       fluxes.coarseRootLoss / params.woodCN;
  // saturationFraction capped between zero and one
  double saturationFraction =
      ctx.carbonSaturation ? unitClip(envi.soilC / params.soilCSaturation)
                           : 0.0;

  // litter
  // The litter org N flux is determined by the carbon fluxes from wood and leaf
  // litter (modified by leaf N resorption), and N loss due to mineralization.
  // N added via fertilization is handled elsewhere.
  fluxes.nOrgLitter =
      fluxes.leafLitter / params.leafCN - fluxes.leafOffNResorption +
      fluxes.woodLitter / params.woodCN - litterMin -
      fluxes.litterToSoil / litterCN + (soilNInputs * saturationFraction);

  // soil
  // The soil org N flux is determined by the carbon flux from the litter pool,
  // carbon fluxes from roots, and N loss due to mineralization
  // (Note: woodCN is used for coarse roots)
  fluxes.nOrgSoil = soilNInputs * (1 - saturationFraction) - soilMin;

  // mineralization
  fluxes.nMin = litterMin + soilMin;
}

// see nitrogen.h
double calcLeafOnNFromC(double leafOnC) {
  return fmax(0.0, leafOnC / params.leafCN - leafOnC / params.woodCN);
}

// see nitrogen.h
double calcPlantNDemandFlux(void) {
  if (!ctx.nitrogenCycle) {
    return 0.0;
  }
  // leaf on "demand" is satisfied entirely (and separately) by the
  // plantStorageN pool, and is not considered demand for the purposes of this
  // function

  // calculate demand from all creation terms
  double creationDemand = fluxes.woodCreation / params.woodCN +
                          fluxes.leafCreation / params.leafCN +
                          fluxes.fineRootCreation / params.fineRootCN +
                          fluxes.coarseRootCreation / params.woodCN;
  return fmax(0.0, creationDemand);
}

// see nitrogen.h
double calcPlantAvailableN(void) {
  // Return total available N for growth; note that we DO consider this time
  // step's fluxes here, unlike most other places. The idea is to prevent
  // negative N pools at the end of the step (but negative in the middle of the
  // step is ok). This is used in the determination of N limitation.
  // Note, though, that we can't really use the incoming N to plantStorageN,
  // as that is not immediately available to offset minN loss.
  double leafOnCFlux = fluxes.leafOnCreation + fluxes.eventLeafOnCreation;
  double leafOnNFlux = calcLeafOnNFromC(leafOnCFlux);
  double unclaimedStorage = envi.plantStorageN - leafOnNFlux * climate->length;
  double nonUptakeDelta = calcMinNNonUptakeFluxes() * climate->length;
  return fmax(0.0, envi.minN + unclaimedStorage + nonUptakeDelta);
}

// see nitrogen.h
double calcMinNNonUptakeFluxes(void) {
  return fluxes.nMin - fluxes.nVolatilization - fluxes.nLeaching;
}

// see nitrogen.h
double calcUnclaimedStorageN(void) {
  double leafOnCFlux = fluxes.leafOnCreation + fluxes.eventLeafOnCreation;
  double leafOnNFlux = calcLeafOnNFromC(leafOnCFlux);
  double unclaimedStorage = envi.plantStorageN - leafOnNFlux * climate->length;
  // The fmax here should be unnecessary, as the leaf-on demand has been capped
  // by the storage pool - but we'll cover our bases anyway
  return fmax(0.0, unclaimedStorage);
}

// see nitrogen.h
double calcNFixationFrac(void) {
  double nFixationInhibition;
  double denom = params.halfNFixationMax + envi.minN;
  if (denom < TINY) {
    nFixationInhibition = 1;
  } else {
    // Calculate inhibition of N fixation by soil mineral N
    // using down-regulation function with increasing soil min N
    // dimensionless between 0 and 1
    nFixationInhibition = params.halfNFixationMax / denom;
  }
  // Calculate fraction of plant N demand met by fixation
  // dimensionless
  return params.nFixationFracMax * nFixationInhibition;
}

// See nitrogen.h
void calcNFixationAndUptakeFluxes(void) {
  // These values may change later if we are under nitrogen limitation
  double nDemandFlux = calcPlantNDemandFlux();

  // Calculate how much will be covered by the storage pool
  double storageFlux = calcUnclaimedStorageN() / climate->length;

  // Remaining demand for uptake/fixation
  double remDemandFlux = fmax(0.0, nDemandFlux - storageFlux);
  // Now parcel that out between fixation and uptake
  double nFixationFrac = calcNFixationFrac();
  fluxes.nFixation = nFixationFrac * remDemandFlux;
  fluxes.nUptake = (1 - nFixationFrac) * remDemandFlux;
}

void calcNResorptionFluxes(void) {
  // We need to check if we are in a negative growth scenario. It would be nice
  // to check meanNPP directly, but it's not worth refactoring that struct out
  // of sipnet.c
  // So, given that ALL of the creation terms are negative-or-not together
  // (well, technically non-positive-or-not), we can check the sum of the
  // creation terms.
  if (fluxes.woodCreation + fluxes.leafCreation + fluxes.fineRootCreation +
          fluxes.coarseRootCreation <
      0.0) {
    // Note: we want these negative fluxes to INCREASE N resorption
    fluxes.reductionNResorption -=
        (fluxes.leafCreation / params.leafCN +
         fluxes.woodCreation / params.woodCN +
         fluxes.coarseRootCreation / params.woodCN +
         fluxes.fineRootCreation / params.fineRootCN);
  }

  // Leaf litter resorption; at this point, fluxes.leafLitter counts both normal
  // turnover and leaf-off calcs. Note that event leaf off is handled in
  // events.c
  double nResorp =
      params.leafNResorptionFrac * fluxes.leafLitter / params.leafCN;
  fluxes.leafOffNResorption += nResorp;

  // TODO: Should we resorb N from wood litter?
}

// see nitrogen.h
void calcNitrogenFluxes(void) {
  if (ctx.nitrogenCycle) {
    calcNResorptionFluxes();
    calcNVolatilizationFlux();
    calcNLeachingFlux();
    calcNPoolFluxes();
    calcNFixationAndUptakeFluxes();
  }
}

// see nitrogen.h
void updateNitrogenPools(void) {
  // Nitrogen Cycle
  // :: from [5], nitrogen cycle model
  // TBD: add equation numbers once published

  // Storage N changes
  // fluxes nUptake  and nFixation handle part of the demand flux (see
  // calcNFixationAndUptakeFluxes), but we expect the rest to come from the
  // storage pool
  double nDemandFlux = calcPlantNDemandFlux();
  double storageDemandFlux = nDemandFlux - fluxes.nUptake - fluxes.nFixation;
  // leaf-on; fluxes.eventLeafOnCreation is handled in events.c
  double leafOnNFlux = calcLeafOnNFromC(fluxes.leafOnCreation);
  envi.plantStorageN +=
      (fluxes.leafOffNResorption + fluxes.reductionNResorption -
       storageDemandFlux - leafOnNFlux) *
      climate->length;

  // Unmet uptake plus other fluxes go to soil mineral N (note we have one
  // mineral pool for soil+litter).
  // Mineral N additions from fertilization are handled with the events
  double nonUptakeFluxes = calcMinNNonUptakeFluxes();
  envi.minN += (nonUptakeFluxes - fluxes.nUptake) * climate->length;

  // Soil organic N
  envi.soilOrgN += fluxes.nOrgSoil * climate->length;

  // Litter organic N
  envi.litterN += fluxes.nOrgLitter * climate->length;
}

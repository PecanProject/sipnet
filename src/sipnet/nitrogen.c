#include "nitrogen.h"

#include <math.h>

#include "common/context.h"
#include "common/logging.h"

#include "depeffects.h"
#include "state.h"

// The following three CN functions may seem trivial, but worth it to ensure
// the divide-by-zero protection. Note that the calcSoil/LitterCN functions
// should only be called when ctx.nitrogen_cycle is on.
double calcCN(const double c, const double n) {
  double effectiveN = n < TINY ? TINY : n;
  return c / effectiveN;
}
double calcSoilCN(void) { return calcCN(envi.soilC, envi.soilOrgN); }
double calcLitterCN(void) { return calcCN(envi.litterC, envi.litterN); }

/*!
 * Calculate mineral N volatilization flux
 */
void calcNVolatilizationFlux(void) {
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
void calcNLeachingFlux(void) {
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

/*!
 * Calculate plant N demand
 */
double calcPlantNDemand() {
  if (!ctx.nitrogenCycle) {
    return 0.0;
  }

  // leafOnCreation is a transfer from the wood pool so
  // demand should only count the difference in the N
  // between those two pools, hence subtracting
  // params.woodCN from params.leafCN for leafOnCreation
  // Note we can have leafOn creation terms from two places
  double leafOnCreation = fluxes.leafOnCreation + fluxes.eventLeafOnCreation;
  double leafOnDemand =
      leafOnCreation / params.leafCN - leafOnCreation / params.woodCN;
  // calculate demand from all other creation terms
  double creationDemand = fluxes.woodCreation / params.woodCN +
                          fluxes.leafCreation / params.leafCN +
                          fluxes.fineRootCreation / params.fineRootCN +
                          fluxes.coarseRootCreation / params.woodCN;
  // total demand is leafOnDemand plus creationDemand
  double totalDemand = leafOnDemand + creationDemand;
  return totalDemand;
}

/**
 * Calculate all fluxes for soil mineral N EXCEPT uptake
 *
 * This is used to help determine N limitation as well as the final min N flux.
 *
 * @return Sum of non-uptake fluxes for soil mineral N
 */
double calcMinNNonUptakeFluxes(void) {
  return fluxes.nMin - fluxes.nVolatilization - fluxes.nLeaching;
}

/**
 * Calculate the N fixation fraction taking inhibition into account
 *
 * @return N fixation fraction used to compute amount of N fixation
 */
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

/*!
 * Calculate plant N fixation and uptake fluxes, checking for N limitation
 */
void calcNFixationAndUptakeFluxes(void) {
  // First, determine if we are in a nitrogen-limited situation
  double maxDemandFlux = calcPlantNDemand();
  double maxDemand = maxDemandFlux * climate->length;
  double nDemandFlux = maxDemandFlux;

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

    nDemandFlux = calcPlantNDemand();
  }

  // Now, actually calculate fixation and uptake!
  fluxes.nFixation = nFixationFrac * nDemandFlux;
  fluxes.nUptake = (1 - nFixationFrac) * nDemandFlux;
}

/**
 * Calculate nitrogen fluxes for soil and litter pools
 */
void calcNPoolFluxes(void) {
  // C:N ratios for litter and soil, needed in most of the succeeding calcs
  double litterCN = calcLitterCN();
  double soilCN = calcSoilCN();

  // for both litter and soil, mineralization is calculated as heterotrophic
  // respiration divided by the C:N ratio of that pool.
  double litterMin = fluxes.rLitter / litterCN;
  double soilMin = fluxes.rSoil / soilCN;

  // litter
  // The litter org N flux is determined by the carbon fluxes from wood and leaf
  // litter, and N loss due to mineralization. N added via fertilization
  // is handled elsewhere.
  fluxes.nOrgLitter = fluxes.leafLitter / params.leafCN +
                      fluxes.woodLitter / params.woodCN - litterMin -
                      fluxes.litterToSoil / litterCN;

  // soil
  // The soil org N flux is determined by the carbon flux from the litter pool,
  // carbon fluxes from roots, and N loss due to mineralization
  // (Note: woodCN is used for coarse roots)
  fluxes.nOrgSoil = fluxes.litterToSoil / litterCN - soilMin +
                    fluxes.fineRootLoss / params.fineRootCN +
                    fluxes.coarseRootLoss / params.woodCN;

  // mineralization
  fluxes.nMin = litterMin + soilMin;
}

// see nitrogen.h
double calcAvailableNitrogen(void) {
  // NOT USED YET
  return 0.0;
}

// see nitrogen.h
void calcNitrogenFluxes(void) {
  calcNVolatilizationFlux();
  calcNLeachingFlux();
  calcNPoolFluxes();
}

// see nitrogen.h
void updateNitrogenPools(void) {
  // Nitrogen Cycle
  // :: from [5], nitrogen cycle model
  // TBD: add equation numbers once published

  // Soil mineral N (note we have one mineral pool for soil+litter)
  // Mineral N additions from fertilization are handled with the events
  double nonUptakeFluxes = calcMinNNonUptakeFluxes();
  envi.minN += (nonUptakeFluxes - fluxes.nUptake) * climate->length;

  // Soil organic N
  envi.soilOrgN += fluxes.nOrgSoil * climate->length;

  // Litter organic N
  envi.litterN += fluxes.nOrgLitter * climate->length;
}

#include "nitrogen.h"

#include <math.h>

#include "common/context.h"
#include "common/logging.h"
#include "common/util.h"

#include "depeffects.h"
#include "model_utils.h"
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

/**
 * Calculate nitrogen demand flux for a given biomass pool
 */
double calcPoolNDemandFlux(double extraN, double creationCFlux, double poolCN) {
  double demand = 0.0;
  // If creation is positive, calculate demand based on that value minus any
  // leftover N from prior negative creation steps (if any).
  // If creation is negative, we have no demand.
  // Turnover, if any, will be handled (later) at this pool's C:N, so shouldn't
  // affect this calc.
  if (creationCFlux > 0.0) {
    // extraN should not ever be negative (it does get checked)
    double extraNFlux = extraN / climate->length;
    demand = fmax(0.0, creationCFlux / poolCN - extraNFlux);
  }
  return demand;
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
  // leaf-on "demand" is handled elsewhere

  // calculate demand from all creation terms
  double creationDemand =
      calcPoolNDemandFlux(nitrogenTrackers.woodExtraN, fluxes.woodCreation,
                          params.woodCN) +
      calcPoolNDemandFlux(nitrogenTrackers.leafExtraN, fluxes.leafCreation,
                          params.leafCN) +
      calcPoolNDemandFlux(nitrogenTrackers.coarseRootExtraN,
                          fluxes.coarseRootCreation, params.woodCN) +
      calcPoolNDemandFlux(nitrogenTrackers.fineRootExtraN,
                          fluxes.fineRootCreation, params.fineRootCN);
  return creationDemand;
}

// see nitrogen.h
double calcPlantAvailableN(void) {
  // Return total available N for growth; note that we DO consider this time
  // step's fluxes here, unlike most other places. The idea is to prevent
  // negative N pools at the end of the step (but negative in the middle of the
  // step is ok). This is used in the determination of N limitation.
  double leafOnCFlux = fluxes.leafOnCreation + fluxes.eventLeafOnCreation;
  double leafOnNFlux = calcLeafOnNFromC(leafOnCFlux);
  // Note: leafOnNFlux has already been limited to not exceed plantStorageN
  double leafOffNFlux =
      fluxes.leafOffNResorption + fluxes.eventLeafOffNResorption;
  double unclaimedStorage =
      envi.plantStorageN + (leafOffNFlux - leafOnNFlux) * climate->length;
  double nonUptakeDelta = calcMinNNonUptakeFluxes() * climate->length;
  return fmax(0.0, envi.minN + unclaimedStorage + nonUptakeDelta);
}

// see nitrogen.h
double calcMinNNonUptakeFluxes(void) {
  return fluxes.nMin - fluxes.nVolatilization - fluxes.nLeaching;
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
  double nFixationFrac = calcNFixationFrac();

  fluxes.nFixation = nFixationFrac * nDemandFlux;
  fluxes.nUptake = (1 - nFixationFrac) * nDemandFlux;
}

// see nitrogen.h
void calcNitrogenFluxes(void) {
  calcNVolatilizationFlux();
  calcNLeachingFlux();
  calcNPoolFluxes();
  calcNFixationAndUptakeFluxes();
}

// see nitrogen.h
void doPlantStorageUpdateFromLeafOn(double leafOnCFlux) {
  double leafOnNFlux = calcLeafOnNFromC(leafOnCFlux);
  envi.plantStorageN -= leafOnNFlux * climate->length;
  // Make sure we didn't overshoot due to counting on leafExtraN
  if (envi.plantStorageN < 0) {
    envi.plantLeafN += envi.plantStorageN;
    envi.plantStorageN = 0;
  }
}

// see nitrogen.h
void updateNitrogenPools(void) {
  // Nitrogen Cycle
  // :: from [5], nitrogen cycle model
  // TBD: add equation numbers once published

  // Biomass pool changes
  // First, standard creation and loss
  double woodNDemandFlux = calcPoolNDemandFlux(
      nitrogenTrackers.woodExtraN, fluxes.woodCreation, params.woodCN);
  double woodNLossFlux = fluxes.woodLitter / params.woodCN;
  envi.plantWoodN += (woodNDemandFlux - woodNLossFlux) * climate->length;

  double leafNDemandFlux = calcPoolNDemandFlux(
      nitrogenTrackers.leafExtraN, fluxes.leafCreation, params.leafCN);
  double leafNLossFlux = fluxes.leafLitter / params.leafCN;
  envi.plantLeafN += (leafNDemandFlux - leafNLossFlux) * climate->length;

  double coarseRootNDemandFlux =
      calcPoolNDemandFlux(nitrogenTrackers.coarseRootExtraN,
                          fluxes.coarseRootCreation, params.woodCN);
  double coarseRootNLossFlux = fluxes.coarseRootLoss / params.woodCN;
  envi.coarseRootN +=
      (coarseRootNDemandFlux - coarseRootNLossFlux) * climate->length;

  double fineRootNDemandFlux =
      calcPoolNDemandFlux(nitrogenTrackers.fineRootExtraN,
                          fluxes.fineRootCreation, params.fineRootCN);
  double fineRootNLossFlux = fluxes.fineRootLoss / params.fineRootCN;
  envi.fineRootN += (fineRootNDemandFlux - fineRootNLossFlux) * climate->length;

  // Now, leaf on creation
  double leafOnC = fluxes.leafOnCreation;
  double leafOnCWood = fluxes.leafOnCreationFromWood;
  double leafOnCRoot = leafOnC - leafOnCWood;
  // N added to leaf, at standard leaf C:N
  envi.plantLeafN += (leafOnC / params.leafCN) * climate->length;
  // N Removed from wood and coarse root; both use same C:N. The difference
  // is handled as part of plantStorageN and uptake.
  envi.plantWoodN -= (leafOnCWood / params.woodCN) * climate->length;
  envi.coarseRootN -= (leafOnCRoot / params.woodCN) * climate->length;

  // Storage N changes
  // First, parcel plantStorageN to leaf-on demand and regular growth demand
  // fluxes.eventLeafOnCreation has already been handled in events.c
  doPlantStorageUpdateFromLeafOn(fluxes.leafOnCreation);

  //  Remaining plantStorageN can go to demand
  double uptake = fluxes.nUptake * climate->length;
  double uptakeFromStorage = fmin(uptake, envi.plantStorageN);
  envi.plantStorageN +=
      fluxes.leafOffNResorption * climate->length - uptakeFromStorage;

  // Unmet uptake plus other fluxes go to soil mineral N (note we have one
  // mineral pool for soil+litter).
  // Mineral N additions from fertilization are handled with the events
  double uptakeFromMinN = uptake - uptakeFromStorage;
  double nonUptakeFluxes = calcMinNNonUptakeFluxes();
  envi.minN += nonUptakeFluxes * climate->length - uptakeFromMinN;

  // Soil organic N
  envi.soilOrgN += fluxes.nOrgSoil * climate->length;

  // Litter organic N
  envi.litterN += fluxes.nOrgLitter * climate->length;
}

NitrogenTrackers nitrogenTrackers;

void initNitrogenTrackers(void) {
  nitrogenTrackers.n2o = 0.0;
  nitrogenTrackers.nLeaching = 0.0;
  nitrogenTrackers.nFixation = 0.0;
  nitrogenTrackers.nUptake = 0.0;

  nitrogenTrackers.leafExtraN = 0.0;
  nitrogenTrackers.woodExtraN = 0.0;
  nitrogenTrackers.coarseRootExtraN = 0.0;
  nitrogenTrackers.fineRootExtraN = 0.0;
}

void updateNitrogenTrackers(void) {
  if (!ctx.nitrogenCycle) {
    return;
  }

  // N cycle trackers
  nitrogenTrackers.n2o = fluxes.nVolatilization * climate->length;
  nitrogenTrackers.nLeaching = fluxes.nLeaching * climate->length;
  nitrogenTrackers.nFixation = fluxes.nFixation * climate->length;
  nitrogenTrackers.nUptake = fluxes.nUptake * climate->length;

  // Nitrogen "overage" from C respiration loss
  nitrogenTrackers.leafExtraN =
      envi.plantLeafN - envi.plantLeafC / params.leafCN;
  nitrogenTrackers.woodExtraN =
      envi.plantWoodN - envi.plantWoodC / params.woodCN;
  nitrogenTrackers.coarseRootExtraN =
      envi.coarseRootN - envi.coarseRootC / params.woodCN;
  nitrogenTrackers.fineRootExtraN =
      envi.fineRootN - envi.fineRootC / params.fineRootCN;

  // None of those overages should be negative - let's check. This will also
  // convert any pesky `-1e-18` or `-0.000` roundoffs to proper zero
  ensureNonNegative(&nitrogenTrackers.leafExtraN, 0, "leafExtraN");
  ensureNonNegative(&nitrogenTrackers.woodExtraN, 0, "woodExtraN");
  ensureNonNegative(&nitrogenTrackers.coarseRootExtraN, 0, "coarseRootExtraN");
  ensureNonNegative(&nitrogenTrackers.fineRootExtraN, 0, "fineRootExtraN");
}

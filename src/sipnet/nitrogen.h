#ifndef NITROGEN_H
#define NITROGEN_H

// Nitrogen cycle related functions

/*!
 * Calculate excess N needed for leaf on events
 */
double calcLeafOnNFromC(double leafOnC);

/*!
 * Calculate plant N demand from biomass creation fluxes
 *
 * @return Total nitrogen demand from plant growth
 */
double calcPlantNDemandFlux(void);

/*!
 * Calculate nitrogen available for plant growth
 *
 * Considers mineral N in soil, unclaimed plant storage N, and non-uptake
 * fluxes that will affect soil mineral N. That is, unlike most other functions,
 * this one considers the current time step's fluxes as well as pools.
 *
 * This function is used to determine whether we are in nitrogen limitation.
 *
 * @return Available N for plant growth
 */
double calcPlantAvailableN(void);

/**
 * Calculate all fluxes for soil mineral N EXCEPT uptake
 *
 * This is used to help determine N limitation as well as the final min N flux.
 *
 * @return Sum of non-uptake fluxes for soil mineral N
 */
double calcMinNNonUptakeFluxes(void);

/**
 * Calculate the N fixation fraction taking inhibition into account
 *
 * @return N fixation fraction used to compute amount of N fixation
 */
double calcNFixationFrac(void);

/*!
 * Calculate plant N fixation and uptake fluxes.
 */
void calcNFixationAndUptakeFluxes(void);

/*!
 * Calculate all nitrogen fluxes
 *
 * This is the general flux calculation wrapper for sipnet.c
 */
void calcNitrogenFluxes(void);

/*!
 * Update the plantStorageN pool for the leaf-on creation flux
 *
 * Reduces plantStorageN based on leaf-on flux, and reduces plantLeafN if there
 * is overshoot from factoring in any available leafExtraN. This function is
 * used both in this nitrogen module and the events module.
 *
 * @param leafOnCFlux leaf-on carbon creation flux
 */
void doPlantStorageUpdateFromLeafOn(double leafOnCFlux);

/*!
 * Update all pools from nitrogen fluxes
 *
 * This is the general pool update wrapper for sipnet.c
 */
void updateNitrogenPools(void);

typedef struct NitrogenTrackersStruct {
  // g N * m^-2 ground area, Mineral N lost to volatilization
  double n2o;
  // g N * m^-2 N leached from soil mineral N pool
  double nLeaching;
  // g N * m^-2 N fixed by plants
  double nFixation;
  // g N * m^-2 N taken up by plants from soil mineral N pool
  double nUptake;

  // Trackers for N in excess of (C / C:N) (g N / m^2)
  double leafExtraN;
  double woodExtraN;
  double coarseRootExtraN;
  double fineRootExtraN;
} NitrogenTrackers;

extern NitrogenTrackers nitrogenTrackers;

/*!
 * Initialize NitrogenTrackers struct for tracking nitrogen data
 */
void initNitrogenTrackers(void);

/*!
 * Perform any needed updates post fluxes-and-pools updates
 */
void updateNitrogenTrackers(void);
#endif  // NITROGEN_H

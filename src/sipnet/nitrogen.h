#ifndef NITROGEN_H
#define NITROGEN_H

// Nitrogen cycle related functions

/*!
 * Calculate excess N needed for leaf on events
 *
 * The input leafOnC can be either a flux or pool measurement; the returned
 * value will match. Note that this will return a negative value in the rare
 * case that params.woodCN < params.leafCN, indicating that some N should be
 * resorbed.
 */
double calcLeafOnNFromC(double leafOnC);

/*!
 * Calculate plant N demand flux from biomass creation fluxes
 *
 * @return Total nitrogen demand flux from plant growth
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
 * Calculate how much of the plant storage N pool is not claimed by leaf-on
 */
double calcUnclaimedStorageN(void);

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
 * Update all pools from nitrogen fluxes
 *
 * This is the general pool update wrapper for sipnet.c
 */
void updateNitrogenPools(void);

/**
 * In the public API for testing
 */
void calcNResorptionFluxes(void);

#endif  // NITROGEN_H

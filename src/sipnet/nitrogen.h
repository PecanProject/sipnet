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
double calcPlantNDemand(void);

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
 *
 * This function is called directly by the leaf-on limitation check
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

#endif  // NITROGEN_H

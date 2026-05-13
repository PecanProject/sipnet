#ifndef NITROGEN_H
#define NITROGEN_H

// Nitrogen cycle related functions

/*!
 * Calculate available nitrogen for this step
 *
 * Takes into account demand, fixation, and...
 *
 * This function should only be called AFTER fluxes are calculated
 *
 * @return available nitrogen
 */
double calcAvailableNitrogen(void);

/*!
 * Calculate plant N demand from biomass creation fluxes
 *
 * @return Total nitrogen demand from plant growth
 */
double calcPlantNDemand(void);

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

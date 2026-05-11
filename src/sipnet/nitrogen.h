#ifndef NITROGEN_H
#define NITROGEN_H

// Nitrogen cycle related functions

/*!
 * Calculate available nitrogen for this step
 *
 * Takes into account demand, fixation, and ...
 *
 * @return available nitrogen
 */
double calcAvailableNitrogen(void);
// ABOVE NOT USED YET

/*!
 * Calculate plant N demand
 */
double calcPlantNDemand();

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
 *
 */
void calcNitrogenFluxes(void);

/*!
 *
 */
void updateNitrogenPools(void);

// TEMP UNTIL REFACTOR COMPLETE
void calcNFixationAndUptakeFluxes(void);

#endif  // NITROGEN_H

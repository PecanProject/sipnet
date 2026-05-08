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

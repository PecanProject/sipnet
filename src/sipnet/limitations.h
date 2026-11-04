#ifndef SIPNET_LIMITATIONS_H
#define SIPNET_LIMITATIONS_H

/**
 * Check for complex limitations
 *
 * Check for complex situations:
 * - nitrogen limitation
 */
void checkLimitations(void);

/**
 * Check for carbon and nitrogen limitations for leaf-on events
 *
 * This function is called where leaf-on events are processed
 *
 * @param[inout] leafOnCDemand pointer to leaf-on demand variable, which may be
 *               modified if limitation is in effect (g C m^-2)
 */
void checkLeafOnLimitation(double *leafOnCDemand);

/** Check for carbon-specific limitations
 *
 * Check carbon limits that should be dealt with before nitrogen fluxes are
 * calculated
 */
void checkCarbonLimitations(void);

#endif  // SIPNET_LIMITATIONS_H

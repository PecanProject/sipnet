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
 * This function is called where leaf-on flux is calculated
 *
 * @param[inout] leafOnFlux pointer to leaf-on flux variable, which may be
 *               modified if limitation is in effect
 */
void checkLeafOnLimitation(double *leafOnFlux);

#endif  // SIPNET_LIMITATIONS_H

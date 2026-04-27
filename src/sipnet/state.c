// Definition of global state variables

#include <math.h>

#include "state.h"

// linked list of climate variables
// one node for each time step
// these climate data are read in from a file
ClimateNode *firstClimate;  // pointer to first climate
ClimateNode *climate;  // current climate
Params params;
Envi envi;  // state variables
Trackers trackers;
PhenologyTrackers phenologyTrackers;
Fluxes fluxes;

double getPlantWoodCTotal(void) {
  return envi.plantWoodC + envi.plantWoodCStorageDelta;
}

double getStorageBackedWoodCarbon(void) {
  return fmax(envi.plantWoodC, getPlantWoodCTotal());
}

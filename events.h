#ifndef EVENTS_H
#define EVENTS_H

typedef enum EventType {
  FERTILIZATION,
  HARVEST,
  IRRIGATION,
  PLANTING,
  TILLAGE,
  UNKNOWN_EVENT
} event_type_t;

typedef enum IrrigationLocation
{
  CANOPY = 0,
  SOIL = 1
} irrigation_location_t;

typedef struct HarvestParams {
  double fractionRemovedAbove;
  double fractionRemovedBelow;
  double fractionTransferredAbove; // to surface litter pool
  double fractionTransferredBelow; // to soil litter pool
} HarvestParams;

typedef struct IrrigationParams {
  double amountAdded;
  irrigation_location_t location;
} IrrigationParams;

typedef struct FertilizationParams {
  double orgN;
  double orgC;
  double minN;
  //double nh4_no3_frac; for two-pool version
} FertilizationParams;

typedef struct PlantingParams {
  int emergenceLag;
  double addedC;
  double addedN;
} PlantingParams;

typedef struct TillageParams {
  double fractionLitterTransferred;
  double somDecompModifier;
  double litterDecompModifier;
} TillageParams;

typedef struct EventNode EventNode;
struct EventNode {
  event_type_t type;
  int loc, year, day;
  void *eventParams;
  EventNode *nextEvent;
};

/* Read event data from input filename (canonically events.in)
 *
 * Format: returned data is structured as an array of EventNode pointers, indexed by
 * location. Each element of the array is the first event for that location (or null
 * if there are no events). It is assumed that the events are ordered first by location
 * and then by year and day.
 */
EventNode** readEventData(char *eventFile, int numLocs);


#endif //EVENTS_H

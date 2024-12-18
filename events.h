//
// Created by Michael J Longfritz on 11/8/24.
//

#ifndef EVENTS_H
#define EVENTS_H

#include "modelStructures.h"

enum EventType {
  FERTILIZATION,
  HARVEST,
  IRRIGATION,
  PLANTING,
  TILLAGE,
  UNKNOWN_EVENT
};

typedef struct HarvestParams {
  double fractionRemovedAbove;
  double fractionRemovedBelow;
  double fractionTransferredAbove; // to surface litter pool
  double fractionTransferredBelow; // to soil litter pool
} HarvestParams;

typedef struct IrrigationParams {
  double amountAdded;
  int location; // 0=canopy, 1=soil
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
  enum EventType type;
  int loc, year, day;
  void *eventParams;
  EventNode *nextEvent;
};

/* Read event data from <FILENAME>.event
 *
 * Format: returned data is structured as an array of EventNode pointers, indexed by
 * location. Each element of the array is the first event for that location (or null
 * if there are no events). It is assumed that the events are in chrono order.
 */
EventNode** readEventData(char *eventFile, int numLocs);


#endif //EVENTS_H

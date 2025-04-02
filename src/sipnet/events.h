#ifndef EVENTS_H
#define EVENTS_H

#include <stdarg.h>

#include "common/util.h"

#define EVENT_OUT_FILE "events.out"

typedef enum EventType {
  FERTILIZATION,
  HARVEST,
  IRRIGATION,
  PLANTING,
  TILLAGE,
  UNKNOWN_EVENT
} event_type_t;

const char *eventTypeToString(event_type_t type);

typedef enum IrrigationMethod {
  CANOPY = 0,
  SOIL = 1,
  FLOOD = 2  // NOLINT placeholder, not supported yet
} irrigation_method_t;

#define NUM_HARVEST_PARAMS 4
typedef struct HarvestParams {
  double fractionRemovedAbove;
  double fractionRemovedBelow;
  double fractionTransferredAbove;  // to surface litter pool
  double fractionTransferredBelow;  // to soil litter pool
} HarvestParams;

#define NUM_IRRIGATION_PARAMS 2
typedef struct IrrigationParams {
  double amountAdded;
  irrigation_method_t method;
} IrrigationParams;

#define NUM_FERTILIZATION_PARAMS 3
typedef struct FertilizationParams {
  double orgN;
  double orgC;
  double minN;
  // double nh4_no3_frac; for two-pool version
} FertilizationParams;

#define NUM_PLANTING_PARAMS 3
typedef struct PlantingParams {
  int emergenceLag;
  double addedC;
  double addedN;
} PlantingParams;

#define NUM_TILLAGE_PARAMS 3
typedef struct TillageParams {
  double fractionLitterTransferred;
  double somDecompModifier;
  double litterDecompModifier;
} TillageParams;

#define NUM_EVENT_CORE_PARAMS 4
typedef struct EventNode EventNode;
struct EventNode {
  event_type_t type;
  int loc, year, day;
  void *eventParams;
  EventNode *nextEvent;
};

/* Read event data from input filename (canonically events.in)
 *
 * Format: returned data is structured as an array of EventNode pointers,
 * indexed by location. Each element of the array is the first event for that
 * location (or null if there are no events). It is assumed that the events are
 * ordered first by location and then by year and day.
 */
EventNode **readEventData(char *eventFile, int numLocs);

FILE *openEventOutFile(void);

void writeEventOut(FILE *out, EventNode *event, const char *format, ...);

void closeEventOutFile(FILE *file);

#endif  // EVENTS_H

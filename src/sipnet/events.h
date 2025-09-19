#ifndef EVENTS_H
#define EVENTS_H

#include <stdarg.h>

#include "common/util.h"

#define EVENT_IN_FILE "events.in"
#define EVENT_OUT_FILE "events.out"

typedef enum EventType {
  FERTILIZATION,
  HARVEST,
  IRRIGATION,
  PLANTING,
  TILLAGE,
  UNKNOWN_EVENT
} event_type_t;

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
} FertilizationParams;

#define NUM_PLANTING_PARAMS 4
typedef struct PlantingParams {
  double leafC;
  double woodC;
  double fineRootC;
  double coarseRootC;
} PlantingParams;

#define NUM_TILLAGE_PARAMS 1
typedef struct TillageParams {
  double tillageEffect;
} TillageParams;
// Tillage effect threshold, below which we call it done
#define TILLAGE_THRESHOLD 0.01
// Tillage effect decay factor
#define TILLAGE_DECAY_FACTOR (1 / 30.0)

#define NUM_EVENT_CORE_PARAMS 3
typedef struct EventNode EventNode;
struct EventNode {
  event_type_t type;
  int year, day;
  void *eventParams;
  EventNode *nextEvent;
};

// Global event variables

extern EventNode *gEvents;
extern EventNode *gEvent;

/*!
 * Convert event enum value to corresponding string
 *
 * @param type enum value representing the event
 * @return string version of event enum type
 */
const char *eventTypeToString(event_type_t type);

/*!
 * Convert event string to corresponding enum value
 *
 * @param string string version of event enum type
 * @return enum value representing the event
 */
event_type_t eventStringToType(const char *eventTypeStr);

/*!
 * Read event data from input filename (canonically events.in)
 *
 * Format: returned data is structured as an linked list of EventNode pointers.
 * It is assumed that the events are ordered by year and day.
 */
EventNode *readEventData(char *eventFile);

/*!
 * Open the event output file and optionally write a header row
 * @param printHeader Flag, non-zero value means write a header row
 * @return FILE pointer to output file
 */
void openEventOutFile(int printHeader);

/*!
 * \brief Write a line to events.out for a single oneEvent
 *
 * Writes a single oneEvent to events.out. This is a variadic function which
 * expects to receive 2*numParams values in (char*, double) pairs after the
 * numParams argument.
 *
 * Output format:
 *
 * year day event_type \<param_name>=\<delta>[,\<param_name>=\<delta>,...]
 *
 * \param out       FILE pointer to events.out
 * \param oneEvent     Pointer to oneEvent node
 * \param numParams Number of param/value pairs to write
 * \param ...       Pairs of (char*, double) arguments to write, 2*numParams
 *                  values
 */
void writeEventOut(EventNode *oneEvent, int numParams, ...);

/*!
 * Close the event output file
 * @param file FILE pointer for output file
 */
void closeEventOutFile(void);

/*!
 * Read in event data for all the model runs
 *
 * Read in event data from a file with the following specification:
 * - one line per event
 * - all events are ordered by year/day ascending
 *
 * @param eventFile Name of file containing event data
 */
void initEvents(char *eventFile, int printHeader);

/*!
 * Initialize global event pointer
 */
void setupEvents(void);

/*!
 * Set all event fluxes to zero
 *
 * Reset all event fluxes to zero in preparation for the next climate step.
 */
void resetEventFluxes(void);

/*!
 * \brief Process events for current location/year/day
 *
 * For a given year and day (as determined by the global `climate`
 * pointer), process all events listed in the global `events` pointer for the
 * referenced location.
 *
 * For each event, modify flux variables according to the model for that event,
 * and write a row to events.out listing the modified variables and the delta
 * applied.
 */
void processEvents(void);

/*!
 * Update relevant environment pools after event fluxes have been calculated
 */
void updatePoolsForEvents(void);

/*!
 * Deallocate space used for events linked list
 */
void freeEventList(void);

// Variables to track events with lingering effects
typedef struct EventTrackerStruct {
  // Tillage effect on Rh; exponentially decays at each time step by a factor
  // equal to exp(-delta_t / 30)
  double d_till_mod;
} EventTrackers;

extern EventTrackers eventTrackers;

/*!
 * Initialize EventTrackers struct for tracking lingering event effects
 */
void initEventTrackers(void);

/*!
 * Perform any needed udpates post fluxes-and-pools updates
 */
void updateEventTrackers(void);

#endif  // EVENTS_H

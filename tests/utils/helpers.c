// Helper functions for setting up and processing events

// NOTE: don't include sipnet.c as well as this file, it won't compile

#include "common/context.h"
#include "sipnet/sipnet.c"

void prepTypesTest(void) {
  // Setup context
  initContext();
  updateIntContext("events", 1, CTX_TEST);

  // set up dummy climate
  climate = (ClimateNode *)malloc(sizeof(ClimateNode));
  climate->year = 2024;
  climate->day = 70;
  climate->length = 0.125;
}

void procEvents(void) {
  resetFluxes();
  processEvents();
  updatePoolsForEvents();
}

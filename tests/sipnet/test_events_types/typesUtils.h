#ifndef SIPNET_TYPESUTILS_H
#define SIPNET_TYPESUTILS_H

#include "common/context.h"
#include "sipnet/sipnet.c"

void prepTypesTest() {
  // Setup context
  initContext();
  updateIntContext("events", 1, CTX_TEST);

  // set up dummy climate
  climate = (ClimateNode *)malloc(sizeof(ClimateNode));
  climate->year = 2024;
  climate->day = 70;
  climate->length = 0.125;
}

#endif  // SIPNET_TYPESUTILS_H

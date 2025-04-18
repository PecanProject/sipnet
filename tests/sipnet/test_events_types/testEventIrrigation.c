
#include <stdio.h>
#include <stdlib.h>

#include "modelStructures.h"  // NOLINT
#include "utils/tUtils.h"
#include "sipnet/sipnet.c"

int checkOutput(double soilWater, double immedEvap) {
  int status = 0;
  if (!compareDoubles(soilWater, envi.soilWater)) {
    printf("Soil water is %f, expected %f\n", envi.soilWater, soilWater);
    status = 1;
  }
  if (!compareDoubles(immedEvap, fluxes.immedEvap)) {
    printf("Immed evap is %f, expected %f\n", fluxes.immedEvap, immedEvap);
    status = 1;
  }
  return status;
}

int run(void) {
  int numLocs = 1;
  int status = 0;

  // set up dummy climate
  climate = (ClimateNode *)malloc(numLocs * sizeof(ClimateNode));
  climate->year = 2024;
  climate->day = 70;

  // set up dummy envi/fluxes/params
  envi.soilWater = 0;
  fluxes.immedEvap = 0;
  params.immedEvapFrac = 0.5;

  //// ONE IRRIGATION EVENT
  // amount 5, method 1 (soil)
  initEvents("events_one_irrig.in", numLocs);
  setupEvents(0);
  processEvents();
  // should have 5 going to the soil
  status |= checkOutput(5, 0);

  //// TWO IRRIGATION EVENTS
  // amount 3, method 1 (soil)
  // amount 4, method 0 (canopy)
  initEvents("events_two_irrig.in", numLocs);
  setupEvents(0);
  processEvents();
  // event 1: 3 to soil
  // event 2: 2=4*0.5 to evap, the rest (2) to soil
  // (plus the five from the test above)
  status |= checkOutput(10, 2);

  free(climate);

  return status;
}

int main(void) {
  printf("Starting run()\n");
  int status = run();
  if (status) {
    printf("FAILED testEventIrrigation with status %d\n", status);
    exit(status);
  }

  printf("PASSED testEventIrrigation\n");
}

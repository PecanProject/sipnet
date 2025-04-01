#include <stdio.h>
#include <stdlib.h>

#include "modelStructures.h"  // NOLINT
#include "utils/tUtils.h"
#include "sipnet/sipnet.c"

int checkOutput(double leafC, double woodC, double fineC, double coarseC) {
  int status = 0;
  if (!compareDoubles(leafC, envi.plantLeafC)) {
    printf("Plant leaf C is %f, expected %f\n", envi.plantLeafC, leafC);
    status = 1;
  }
  if (!compareDoubles(woodC, envi.plantWoodC)) {
    printf("Plant wood C is %f, expected %f\n", envi.plantWoodC, woodC);
    status = 1;
  }
  if (!compareDoubles(fineC, envi.fineRootC)) {
    printf("Fine root C is %f, expected %f\n", envi.fineRootC, fineC);
    status = 1;
  }
  if (!compareDoubles(coarseC, envi.coarseRootC)) {
    printf("Coarse root C is %f, expected %f\n", envi.coarseRootC, coarseC);
    status = 1;
  }
  return status;
}

void initEnv(void) {
  envi.plantLeafC = 1;
  envi.plantWoodC = 2;
  envi.fineRootC = 3;
  envi.coarseRootC = 4;
}

int run(void) {
  int numLocs = 1;
  int status = 0;
  double lai = LAI_AT_EMERGENCE;

  // set up dummy climate
  climate = malloc(numLocs * sizeof(ClimateNode));
  climate->year = 2024;
  climate->day = 70;

  // set up dummy envi/fluxes/params
  params.leafCSpWt = 5.0;
  params.leafAllocation = 0.1;
  params.woodAllocation = 0.5;
  params.fineRootAllocation = 0.1;
  ensureAllocation();

  // init values
  initEnv();

  //// ONE PLANTING EVENT
  events = readEventData("events_one_planting.in", numLocs);
  setupEvents(0);
  processEvents();
  // should have 7.5g C, allocated as above to the four pools
  double totC = lai * 5 / 0.1;
  status |= checkOutput(1 + totC * 0.1, 2 + totC * 0.5, 3 + totC * 0.1,
                        4 + totC * 0.3);

  //// TWO PLANTING EVENTS
  initEnv();
  events = readEventData("events_two_planting.in", numLocs);
  setupEvents(0);
  processEvents();
  // Should have twice as much as before, as the plantings are the same
  totC *= 2;
  status |= checkOutput(1 + totC * 0.1, 2 + totC * 0.5, 3 + totC * 0.1,
                        4 + totC * 0.3);

  return status;
}

int main(void) {
  printf("Starting run()\n");
  int status = run();
  if (status) {
    printf("FAILED testEventPlanting with status %d\n", status);
    exit(status);
  }

  printf("PASSED testEventPlanting\n");
}

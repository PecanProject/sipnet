#include <stdio.h>
#include <stdlib.h>

#include "modelStructures.h"  // NOLINT
#include "utils/tUtils.h"
#include "sipnet/sipnet.c"

int checkOutput(double litter, double leafC, double woodC, double fineC,
                double coarseC) {
  int status = 0;
  if (!compareDoubles(litter, envi.litter)) {
    printf("Litter C is %f, expected %f\n", envi.litter, litter);
    status = 1;
  }
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
  envi.litter = 1;
  envi.plantLeafC = 2;
  envi.plantWoodC = 3;
  envi.fineRootC = 4;
  envi.coarseRootC = 5;
}

int run(void) {
  int numLocs = 1;
  int status = 0;
  double expLitter, expLeafC, expWoodC, expFineC, expCoarseC;

  // set up dummy climate
  climate = (ClimateNode *)malloc(numLocs * sizeof(ClimateNode));
  climate->year = 2024;
  climate->day = 70;

  // set up dummy envi/fluxes/params
  // None for this test

  // init values
  initEnv();

  //// ONE PLANTING EVENT
  initEvents("events_one_harvest.in", numLocs);
  setupEvents(0);
  processEvents();

  // fracRA = 0.1, fracRB = 0.2, frac TA = 0.3, fracTB = 0.4
  expLitter = 1 + 0.3 * (2 + 3) + 0.4 * (4 + 5);
  expLeafC = 2 * (1 - 0.1 - 0.3);
  expWoodC = 3 * (1 - 0.1 - 0.3);
  expFineC = 4 * (1 - 0.2 - 0.4);
  expCoarseC = 5 * (1 - 0.2 - 0.4);
  status |= checkOutput(expLitter, expLeafC, expWoodC, expFineC, expCoarseC);

  //// TWO HARVEST EVENTS
  initEnv();
  initEvents("events_two_harvest.in", numLocs);
  setupEvents(0);
  processEvents();
  // First event same as above
  expLitter = 1 + 0.3 * (2 + 3) + 0.4 * (4 + 5);
  expLeafC = 2 * (1 - 0.1 - 0.3);
  expWoodC = 3 * (1 - 0.1 - 0.3);
  expFineC = 4 * (1 - 0.2 - 0.4);
  expCoarseC = 5 * (1 - 0.2 - 0.4);
  // Second is all params = 0.25
  expLitter += 0.25 * (expLeafC + expWoodC + expFineC + expCoarseC);
  expLeafC *= 0.5;
  expWoodC *= 0.5;
  expFineC *= 0.5;
  expCoarseC *= 0.5;
  status |= checkOutput(expLitter, expLeafC, expWoodC, expFineC, expCoarseC);

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

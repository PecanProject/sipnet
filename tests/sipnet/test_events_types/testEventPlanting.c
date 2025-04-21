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

  // set up dummy climate
  climate = (ClimateNode *)malloc(numLocs * sizeof(ClimateNode));
  climate->year = 2024;
  climate->day = 70;

  // init values
  initEnv();

  //// ONE PLANTING EVENT
  initEvents("events_one_planting.in", numLocs, 0);
  setupEvents(0);
  processEvents();
  // added: leaf 10, wood 5, fine root 4, coarse root 3
  status |= checkOutput(1 + 10, 2 + 5, 3 + 4, 4 + 3);

  //// TWO PLANTING EVENTS
  initEnv();
  initEvents("events_two_planting.in", numLocs, 1);
  setupEvents(0);
  processEvents();
  // leaf 10+9, wood 5+6, fine root 4+8, coarse root 3+4
  status |= checkOutput(1 + 19, 2 + 11, 3 + 12, 4 + 7);

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

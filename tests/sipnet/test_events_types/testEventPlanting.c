#include <stdio.h>
#include <stdlib.h>

#include "utils/tUtils.h"

#include "typesUtils.h"

int checkOutput(double leafC, double woodC, double fineC, double coarseC) {
  int status = 0;
  if (!compareDoubles(leafC, envi.plantLeafC)) {
    logTest("Plant leaf C is %f, expected %f\n", envi.plantLeafC, leafC);
    status = 1;
  }
  if (!compareDoubles(woodC, envi.plantWoodC)) {
    logTest("Plant wood C is %f, expected %f\n", envi.plantWoodC, woodC);
    status = 1;
  }
  if (!compareDoubles(fineC, envi.fineRootC)) {
    logTest("Fine root C is %f, expected %f\n", envi.fineRootC, fineC);
    status = 1;
  }
  if (!compareDoubles(coarseC, envi.coarseRootC)) {
    logTest("Coarse root C is %f, expected %f\n", envi.coarseRootC, coarseC);
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
  int status = 0;

  prepTypesTest();

  // init values
  initEnv();

  //// ONE PLANTING EVENT
  initEvents("events_one_planting.in", 0);
  setupEvents();
  procEvents();
  // added: leaf 10, wood 5, fine root 4, coarse root 3
  status |= checkOutput(1 + 10, 2 + 5, 3 + 4, 4 + 3);

  //// TWO PLANTING EVENTS
  initEnv();
  initEvents("events_two_planting.in", 1);
  setupEvents();
  procEvents();
  // leaf 10+9, wood 5+6, fine root 4+8, coarse root 3+4
  status |= checkOutput(1 + 19, 2 + 11, 3 + 12, 4 + 7);

  return status;
}

int main(void) {
  logTest("Starting run()\n");
  int status = run();
  if (status) {
    logTest("FAILED testEventPlanting with status %d\n", status);
    exit(status);
  }

  logTest("PASSED testEventPlanting\n");
}

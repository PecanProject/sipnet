#include <stdio.h>
#include <stdlib.h>

#include "utils/tUtils.h"
#include "utils/helpers.c"

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

int checkOutputN(double leafN, double woodN, double fineN, double coarseN) {
  int status = 0;
  if (!compareDoubles(leafN, envi.plantLeafN)) {
    logTest("Plant leaf N is %f, expected %f\n", envi.plantLeafN, leafN);
    status = 1;
  }
  if (!compareDoubles(woodN, envi.plantWoodN)) {
    logTest("Plant wood N is %f, expected %f\n", envi.plantWoodN, woodN);
    status = 1;
  }
  if (!compareDoubles(fineN, envi.fineRootN)) {
    logTest("Fine root N is %f, expected %f\n", envi.fineRootN, fineN);
    status = 1;
  }
  if (!compareDoubles(coarseN, envi.coarseRootN)) {
    logTest("Coarse root N is %f, expected %f\n", envi.coarseRootN, coarseN);
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
  initEvents("events_one_planting.in", "events.out", 0);
  setupEvents();
  procEvents();
  closeEventOutFile();
  // added: leaf 10, wood 5, fine root 4, coarse root 3
  status |= checkOutput(1 + 10, 2 + 5, 3 + 4, 4 + 3);

  //// TWO PLANTING EVENTS
  initEnv();
  initEvents("events_two_planting.in", "events.out", 1);
  setupEvents();
  procEvents();
  closeEventOutFile();
  // leaf 10+9, wood 5+6, fine root 4+8, coarse root 3+4
  status |= checkOutput(1 + 19, 2 + 11, 3 + 12, 4 + 7);

  //// ONE PLANTING EVENT with nitrogen cycle
  // Biomass N pools should be initialized to C/CN and then increased by the
  // planted N (at each pool's respective C:N ratio).
  logTest("Testing one event with nitrogen\n");
  updateIntContext("litterPool", 1, CTX_TEST);
  updateIntContext("nitrogenCycle", 1, CTX_TEST);
  params.leafCN = 20.0;
  params.woodCN = 10.0;
  params.fineRootCN = 40.0;
  initEnv();
  envi.plantLeafN = envi.plantLeafC / params.leafCN;  // 1/20 = 0.05
  envi.plantWoodN = envi.plantWoodC / params.woodCN;  // 2/10 = 0.2
  envi.fineRootN = envi.fineRootC / params.fineRootCN;  // 3/40 = 0.075
  envi.coarseRootN = envi.coarseRootC / params.woodCN;  // 4/10 = 0.4
  initEvents("events_one_planting.in", "events.out", 0);
  setupEvents();
  procEvents();
  closeEventOutFile();
  // C pools as before: leaf+10, wood+5, fine+4, coarse+3
  status |= checkOutput(1 + 10, 2 + 5, 3 + 4, 4 + 3);
  // N pools should increase by planted C / CN for each pool
  // leaf: 0.05 + 10/leafCN = 0.05 + 0.5 = 0.55
  // wood: 0.2  + 5/woodCN  = 0.2  + 0.5 = 0.7
  // fine: 0.075 + 4/fineRootCN = 0.075 + 0.1 = 0.175
  // coarse: 0.4 + 3/woodCN = 0.4 + 0.3 = 0.7
  status |=
      checkOutputN(0.05 + 10.0 / params.leafCN, 0.2 + 5.0 / params.woodCN,
                   0.075 + 4.0 / params.fineRootCN, 0.4 + 3.0 / params.woodCN);

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

#include <stdio.h>
#include <stdlib.h>

#include "utils/tUtils.h"

#include "typesUtils.h"

int checkOutput(double litter, double leafC, double woodC, double fineC,
                double coarseC) {
  int status = 0;
  double curLitter = 0;
  if (ctx.litterPool) {
    logTest("Checking litter pool\n");
    curLitter = envi.litter;
  } else {
    logTest("Checking soil pool\n");
    curLitter = envi.soil;
    litter += 0.5;  // We bumped init soil C to distinguish
  }
  if (!compareDoubles(litter, curLitter)) {
    logTest("Litter/soil C is %f, expected %f\n", curLitter, litter);
    status = 1;
  }
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
  envi.soil = 1.5;
  envi.litter = 1;
  envi.plantLeafC = 2;
  envi.plantWoodC = 3;
  envi.fineRootC = 4;
  envi.coarseRootC = 5;
}

int run(void) {
  int status = 0;
  double expLitter, expLeafC, expWoodC, expFineC, expCoarseC;

  // We will need to switch back and forth between litter pool and soil manually
  prepTypesTest();

  // init values
  initEnv();

  //// ONE PLANTING EVENT
  updateIntContext("litterPool", 0, CTX_TEST);
  logTest("Litter pool is %s\n", ctx.litterPool ? "on" : "off");
  initEvents("events_one_harvest.in", 0);
  setupEvents();
  procEvents();

  // fracRA = 0.1, fracRB = 0.2, frac TA = 0.3, fracTB = 0.4
  expLitter = 1 + 0.3 * (2 + 3) + 0.4 * (4 + 5);  // 1 + 1.5 + 3.6 = 6.1
  expLeafC = 2 * (1 - 0.1 - 0.3);  // 1.2
  expWoodC = 3 * (1 - 0.1 - 0.3);  // 1.8
  expFineC = 4 * (1 - 0.2 - 0.4);  // 1.6
  expCoarseC = 5 * (1 - 0.2 - 0.4);  // 2.0
  status |= checkOutput(expLitter, expLeafC, expWoodC, expFineC, expCoarseC);

  //// TWO HARVEST
  // Ok, so, two harvest events on the same day shouldn't happen (seriously,
  // model it as one harvest) - but we can test the arithmetic here
  updateIntContext("litterPool", 1, CTX_TEST);
  logTest("Litter pool is %s\n", ctx.litterPool ? "on" : "off");
  initEnv();
  initEvents("events_two_harvest.in", 1);
  setupEvents();
  procEvents();
  // Two events are additive
  expLitter = 1 + (0.3 + 0.25) * (2 + 3) + (0.4 + 0.25) * (4 + 5);  // 9.6
  expLeafC = 2 * (1 - 0.1 - 0.3 - 0.25 - 0.25);  // 0.2
  expWoodC = 3 * (1 - 0.1 - 0.3 - 0.25 - 0.25);  // 0.3
  expFineC = 4 * (1 - 0.2 - 0.4 - 0.25 - 0.25);  // -0.4
  expCoarseC = 5 * (1 - 0.2 - 0.4 - 0.25 - 0.25);  // -0.5

  status |= checkOutput(expLitter, expLeafC, expWoodC, expFineC, expCoarseC);

  return status;
}

int main(void) {
  logTest("Starting run()\n");
  int status = run();
  if (status) {
    logTest("FAILED testEventHarvest with status %d\n", status);
    exit(status);
  }

  logTest("PASSED testEventHarvest\n");
}

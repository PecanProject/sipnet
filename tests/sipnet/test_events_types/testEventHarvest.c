#include <stdio.h>
#include <stdlib.h>

#include "utils/tUtils.h"
#include "utils/helpers.c"

int checkBioOutput(double leafC, double woodC, double fineC, double coarseC) {
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

int checkSoilOutput(double soilC, double litterC, double soilOrgN,
                    double litterN) {
  int status = 0;
  if (!compareDoubles(soilC, envi.soilC)) {
    logTest("Soil C is %f, expected %f\n", envi.soilC, soilC);
    status = 1;
  }
  if (!compareDoubles(litterC, envi.litterC)) {
    logTest("Litter C is %f, expected %f\n", envi.litterC, litterC);
    status = 1;
  }
  if (!compareDoubles(soilOrgN, envi.soilOrgN)) {
    logTest("Soil Org N is %f, expected %f\n", envi.soilOrgN, soilOrgN);
    status = 1;
  }
  if (!compareDoubles(litterN, envi.litterN)) {
    logTest("Litter N is %f, expected %f\n", envi.litterN, litterN);
    status = 1;
  }
  return status;
}

void initEnv(void) {
  envi.soilC = 10;
  envi.litterC = 0;
  envi.plantLeafC = 2;
  envi.plantWoodC = 3;
  envi.fineRootC = 4;
  envi.coarseRootC = 5;
  envi.soilOrgN = 0;
  envi.litterN = 0;

  if (ctx.litterPool) {
    envi.litterC = 15;
  }
  if (ctx.nitrogenCycle) {
    envi.soilOrgN = 2.0;
    envi.litterN = 3.0;
    params.woodCN = 10;
    params.leafCN = 20;
    params.fineRootCN = 30;
  }

  // not used here, but accessed
  envi.plantWoodCStorageDelta = 0.0;
  envi.soilWater = 10.0;
  envi.minN = 10.0;
}

int run(void) {
  int status = 0;
  double expSoilC, expLitterC;
  double expLeafC, expWoodC, expFineC, expCoarseC;
  double expSoilOrgN = 0.0;
  double expLitterN = 0.0;

  // We will need to switch back and forth between litter pool and soil manually
  prepTypesTest();

  // init values
  initEnv();

  //// ONE HARVEST EVENT
  logTest("Testing one event\n");
  updateIntContext("litterPool", 0, CTX_TEST);
  updateIntContext("nitrogenCycle", 0, CTX_TEST);
  logTest("Litter pool is %s, nitrogen cycle is %s\n",
          ctx.litterPool ? "on" : "off", ctx.nitrogenCycle ? "on" : "off");
  initEvents("events_one_harvest.in", "events.out", 0);
  setupEvents();
  procEvents();
  closeEventOutFile();
  // fracRA = 0.1, fracRB = 0.2, frac TA = 0.3, fracTB = 0.4
  expSoilC = 10 + 0.3 * (2 + 3) + 0.4 * (4 + 5);  // 10 + 1.5 + 3.6 = 15.1
  expLitterC = 0.0;  // litter pool off
  expLeafC = 2 * (1 - 0.1 - 0.3);  // 1.2
  expWoodC = 3 * (1 - 0.1 - 0.3);  // 1.8
  expFineC = 4 * (1 - 0.2 - 0.4);  // 1.6
  expCoarseC = 5 * (1 - 0.2 - 0.4);  // 2.0
  status |= checkBioOutput(expLeafC, expWoodC, expFineC, expCoarseC);
  status |= checkSoilOutput(expSoilC, expLitterC, expSoilOrgN, expLitterN);

  //// TWO HARVEST EVENTS
  // Ok, so, two harvest events on the same day shouldn't happen (seriously,
  // model it as one harvest) - but we can test the arithmetic here
  // fracRA = 0.1, fracRB = 0.2, frac TA = 0.3, fracTB = 0.4
  // fracRA = 0.2, fracRB = 0.1, frac TA = 0.2, fracTB = 0.1
  logTest("Testing two events\n");
  updateIntContext("litterPool", 1, CTX_TEST);
  updateIntContext("nitrogenCycle", 1, CTX_TEST);
  logTest("Litter pool is %s, nitrogen cycle is %s\n",
          ctx.litterPool ? "on" : "off", ctx.nitrogenCycle ? "on" : "off");
  initEnv();
  initEvents("events_two_harvest.in", "events.out", 1);
  setupEvents();
  procEvents();
  closeEventOutFile();
  // Two events are additive
  expSoilC = 10 + (0.4 + 0.1) * (4 + 5);
  expLitterC = 15 + (0.3 + 0.2) * (2 + 3);
  expLeafC = 2 * (1 - 0.1 - 0.3 - 0.2 - 0.2);
  expWoodC = 3 * (1 - 0.1 - 0.3 - 0.2 - 0.2);
  expFineC = 4 * (1 - 0.2 - 0.4 - 0.1 - 0.1);
  expCoarseC = 5 * (1 - 0.2 - 0.4 - 0.1 - 0.1);
  expSoilOrgN = 2 + (4 * (0.4 + 0.1)) / params.fineRootCN +
                (5 * (0.4 + 0.1)) / params.woodCN;
  expLitterN =
      3 + (3 * (0.3 + 0.2)) / params.woodCN + (2 * (0.3 + 0.2)) / params.leafCN;

  status |= checkBioOutput(expLeafC, expWoodC, expFineC, expCoarseC);
  status |= checkSoilOutput(expSoilC, expLitterC, expSoilOrgN, expLitterN);

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

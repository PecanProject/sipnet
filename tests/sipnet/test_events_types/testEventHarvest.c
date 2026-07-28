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

int checkBioNOutput(double leafN, double woodN, double fineN, double coarseN) {
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
    envi.plantLeafN = envi.plantLeafC / params.leafCN;
    envi.plantWoodN = envi.plantWoodC / params.woodCN;
    envi.fineRootN = envi.fineRootC / params.fineRootCN;
    envi.coarseRootN = envi.coarseRootC / params.woodCN;
  }

  // not used here, but accessed
  envi.plantCAccountingDelta = 0.0;
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
  // Litter/soil N uses biomass N pools (plantLeafN + plantWoodN = totalAbove)
  // Both events use the initial pool values since pools update after all fluxes
  double initLeafN = 2.0 / params.leafCN;  // 2/20 = 0.1
  double initWoodN = 3.0 / params.woodCN;  // 3/10 = 0.3
  double initFineN = 4.0 / params.fineRootCN;  // 4/30
  double initCoarseN = 5.0 / params.woodCN;  // 5/10 = 0.5
  double totalAbove = initLeafN + initWoodN;
  double totalBelow = initFineN + initCoarseN;
  expSoilOrgN = 2 + (0.4 + 0.1) * totalBelow;
  expLitterN = 3 + (0.3 + 0.2) * totalAbove;
  // Biomass N changes: both events reduce N at their combined (fracRA + fracTA)
  // rates Event 1: fracRA=0.1, fracTA=0.3 -> above reduction = 0.4
  //          fracRB=0.2, fracTB=0.4 -> below reduction = 0.6
  // Event 2: fracRA=0.2, fracTA=0.2 -> above reduction = 0.4
  //          fracRB=0.1, fracTB=0.1 -> below reduction = 0.2
  double expLeafN = initLeafN * (1 - (0.1 + 0.3) - (0.2 + 0.2));
  double expWoodN = initWoodN * (1 - (0.1 + 0.3) - (0.2 + 0.2));
  double expFineN = initFineN * (1 - (0.2 + 0.4) - (0.1 + 0.1));
  double expCoarseN = initCoarseN * (1 - (0.2 + 0.4) - (0.1 + 0.1));

  status |= checkBioOutput(expLeafC, expWoodC, expFineC, expCoarseC);
  status |= checkSoilOutput(expSoilC, expLitterC, expSoilOrgN, expLitterN);
  status |= checkBioNOutput(expLeafN, expWoodN, expFineN, expCoarseN);

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

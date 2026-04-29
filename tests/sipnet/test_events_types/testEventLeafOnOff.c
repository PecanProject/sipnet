#include <stdio.h>
#include <stdlib.h>

#include "utils/tUtils.h"
#include "utils/helpers.c"

int checkLeafOn(const char *stage, double expWoodC, double expLeafC) {
  int status = 0;
  if (!compareDoubles(expWoodC, envi.plantWoodC)) {
    logTest("%s: plantWoodC is %f, expected %f\n", stage, envi.plantWoodC,
            expWoodC);
    status = 1;
  }
  if (!compareDoubles(expLeafC, envi.plantLeafC)) {
    logTest("%s: plantLeafC is %f, expected %f\n", stage, envi.plantLeafC,
            expLeafC);
    status = 1;
  }
  return status;
}

int checkLeafOff(const char *stage, double expLeafC, double expPoolC,
                 double expLitterN) {
  int status = 0;
  double curPoolC = ctx.litterPool ? envi.litterC : envi.soilC;
  if (!compareDoubles(expLeafC, envi.plantLeafC)) {
    logTest("%s: plantLeafC is %f, expected %f\n", stage, envi.plantLeafC,
            expLeafC);
    status = 1;
  }
  if (!compareDoubles(expPoolC, curPoolC)) {
    logTest("%s: %s is %f, expected %f\n", stage,
            ctx.litterPool ? "litterC" : "soilC", curPoolC, expPoolC);
    status = 1;
  }
  if (!compareDoubles(expLitterN, envi.litterN)) {
    logTest("%s: litterN is %f, expected %f\n", stage, envi.litterN,
            expLitterN);
    status = 1;
  }
  return status;
}

void initEnv(double woodC, double leafC) {
  envi.plantWoodC = woodC;
  envi.plantLeafC = leafC;
  envi.litterC = 0.0;
  envi.soilC = 0.0;
  envi.litterN = 0.0;
}

int run(void) {
  int status = 0;

  prepTypesTest();
  updateIntContext("litterPool", 1, CTX_TEST);
  updateIntContext("gdd", 0, CTX_TEST);

  // Set params needed for leaf on/off events
  params.leafGrowth = 3.0;
  params.fracLeafFall = 0.5;
  params.leafCN = 30.0;
  params.leafOnDay = 0;
  params.leafOffDay = 0;

  //// ONE LEAFON EVENT
  logTest("Testing one leafon event\n");
  initEnv(10.0, 0.0);
  resetFluxes();
  initEvents("events_one_leafon.in", "events.out", 0);
  setupEvents();
  procEvents();
  closeEventOutFile();

  // leafOn = params.leafGrowth = 3.0
  // wood decreases by 3.0, leaf increases by 3.0
  status |= checkLeafOn("one leafon", 10.0 - 3.0, 0.0 + 3.0);

  //// TWO LEAFON EVENTS
  logTest("Testing two leafon events\n");
  initEnv(10.0, 0.0);
  resetFluxes();
  initEvents("events_two_leafon.in", "events.out", 0);
  setupEvents();
  procEvents();
  closeEventOutFile();

  // Two leafOn events: total 2 * 3.0 = 6.0 from wood to leaf
  status |= checkLeafOn("two leafon", 10.0 - 6.0, 0.0 + 6.0);

  //// ONE LEAFOFF EVENT (without nitrogen cycle)
  logTest("Testing one leafoff event (no nitrogen cycle)\n");
  initEnv(5.0, 10.0);
  resetFluxes();
  initEvents("events_one_leafoff.in", "events.out", 0);
  setupEvents();
  procEvents();
  closeEventOutFile();

  // leafOff = plantLeafC * fracLeafFall = 10.0 * 0.5 = 5.0
  // leaf decreases by 5.0, litter increases by 5.0; no N (nitrogenCycle off)
  status |= checkLeafOff("one leafoff (no N)", 10.0 - 5.0, 0.0 + 5.0, 0.0);

  //// ONE LEAFOFF EVENT (with nitrogen cycle)
  logTest("Testing one leafoff event (with nitrogen cycle)\n");
  updateIntContext("anaerobic", 1, CTX_TEST);
  updateIntContext("waterHResp", 1, CTX_TEST);
  updateIntContext("nitrogenCycle", 1, CTX_TEST);
  initEnv(5.0, 10.0);
  resetFluxes();
  initEvents("events_one_leafoff.in", "events.out", 0);
  setupEvents();
  procEvents();
  closeEventOutFile();

  // leafOff = 10.0 * 0.5 = 5.0; N = leafOff / leafCN = 5.0 / 30.0
  status |=
      checkLeafOff("one leafoff (with N)", 10.0 - 5.0, 0.0 + 5.0, 5.0 / 30.0);

  //// ONE LEAFOFF EVENT (without litter pool)
  logTest("Testing one leafoff event (no litter pool)\n");
  updateIntContext("litterPool", 0, CTX_TEST);
  updateIntContext("nitrogenCycle", 0, CTX_TEST);
  updateIntContext("anaerobic", 0, CTX_TEST);
  updateIntContext("waterHResp", 0, CTX_TEST);
  initEnv(5.0, 10.0);
  resetFluxes();
  initEvents("events_one_leafoff.in", "events.out", 0);
  setupEvents();
  procEvents();
  closeEventOutFile();

  // Without litter pool, leaf C goes to soil C instead
  status |=
      checkLeafOff("one leafoff (no litter pool)", 10.0 - 5.0, 0.0 + 5.0, 0.0);

  return status;
}

int main(void) {
  logTest("Starting run()\n");
  int status = run();
  if (status) {
    logTest("FAILED testEventLeafOnOff with status %d\n", status);
    exit(status);
  }

  logTest("PASSED testEventLeafOnOff\n");
}

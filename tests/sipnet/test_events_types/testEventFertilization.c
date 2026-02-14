#include <stdio.h>
#include <stdlib.h>

#include "utils/tUtils.h"

#include "typesUtils.h"

int checkOutput(double orgN, double litterC, double minN) {
  int status = 0;
  double curLitterC = 0;
  double curOrgN = envi.litterN;
  double curMinN = envi.minN;
  if (ctx.litterPool) {
    logTest("Checking litter pool for carbon addition\n");
    curLitterC = envi.litterC;
  } else {
    logTest("Checking soil pool for carbon addition\n");
    curLitterC = envi.soilC;
    // We bumped init soil C to distinguish
    litterC += 0.5;
  }
  if (!compareDoubles(orgN, curOrgN)) {
    logTest("Litter org N is %f, expected %f\n", curOrgN, orgN);
    status = 1;
  }
  if (!compareDoubles(litterC, curLitterC)) {
    logTest("Litter/soil C is %f, expected %f\n", curLitterC, litterC);
    status = 1;
  }
  if (!compareDoubles(minN, curMinN)) {
    logTest("Mineral N is %f, expected %f\n", curMinN, minN);
    status = 1;
  }

  return status;
}

void initEnv(double minN, double orgN) {
  envi.soilC = 1.5;
  envi.litterC = 1;
  envi.minN = minN;
  envi.soilOrgN = 0;
  envi.litterN = orgN;
}

int run(void) {
  int status = 0;
  double expLitterC, expMinN, expOrgN;

  // We will need to switch back and forth between litter pool and soil manually
  prepTypesTest();

  // init values
  initEnv(0, 0);

  //// ONE PLANTING EVENT
  // We can't turn nitrogen_cycle on when litter_pool is off
  updateIntContext("litterPool", 0, CTX_TEST);
  logTest("Litter pool is %s\n", ctx.litterPool ? "on" : "off");
  logTest("Nitrogen cycle is %s\n", ctx.nitrogenCycle ? "on" : "off");
  initEvents("events_one_fert.in", 0);
  setupEvents();
  procEvents();

  // First fert: (15-5-10)
  expOrgN = 0;  // nitrogen cycle is off
  expLitterC = 1 + 5;
  expMinN = 0;  // nitrogen cycle is off
  status |= checkOutput(expOrgN, expLitterC, expMinN);

  //// TWO HARVEST EVENTS
  updateIntContext("litterPool", 1, CTX_TEST);
  updateIntContext("nitrogenCycle", 1, CTX_TEST);
  logTest("Litter pool is %s\n", ctx.litterPool ? "on" : "off");
  logTest("Nitrogen cycle is %s\n", ctx.nitrogenCycle ? "on" : "off");
  initEnv(2, 3);
  initEvents("events_two_fert.in", 1);
  setupEvents();
  procEvents();

  // First event same as above (15-5-10)
  expOrgN = 3 + 15;
  expLitterC = 1 + 5;
  expMinN = 2 + 10;

  // Second fert (5-2-3)
  expOrgN += 5;
  expLitterC += 2;
  expMinN += 3;
  status |= checkOutput(expOrgN, expLitterC, expMinN);

  return status;
}

int main(void) {
  logTest("Starting run()\n");
  int status = run();
  if (status) {
    logTest("FAILED testEventFertilization with status %d\n", status);
    exit(status);
  }

  logTest("PASSED testEventFertilization\n");
}

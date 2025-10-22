#include <stdio.h>
#include <stdlib.h>

#include "utils/tUtils.h"

#include "typesUtils.h"

int checkOutput(double litterC, double minN) {
  int status = 0;
  double curLitterC = 0;
  double curMinN = envi.minN;
  if (ctx.litterPool) {
    logTest("Checking litter pool\n");
    curLitterC = envi.litter;
  } else {
    logTest("Checking soil pool\n");
    curLitterC = envi.soil;
    litterC += 0.5;  // We bumped init soil C to distinguish
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

void initEnv(void) {
  envi.soil = 1.5;
  envi.litter = 1;
  envi.minN = 0;
  // envi.orgN, eventually
}

int run(void) {
  int status = 0;
  double expLitterC, expMinN;

  // We will need to switch back and forth between litter pool and soil manually
  prepTypesTest();

  // init values
  initEnv();

  //// ONE PLANTING EVENT
  updateIntContext("litterPool", 0, CTX_TEST);
  logTest("Litter pool is %s\n", ctx.litterPool ? "on" : "off");
  initEvents("events_one_fert.in", 0);
  setupEvents();
  procEvents();

  // First fert: (15-5-10)
  expLitterC = 1 + 5;
  // litterN + 15
  expMinN = 0 + 10;
  status |= checkOutput(expLitterC, expMinN);

  //// TWO HARVEST EVENTS
  updateIntContext("litterPool", 1, CTX_TEST);
  logTest("Litter pool is %s\n", ctx.litterPool ? "on" : "off");
  initEnv();
  initEvents("events_two_fert.in", 1);
  setupEvents();
  procEvents();
  // First event same as above (15-5-10)
  expLitterC = 1 + 5;
  expMinN = 0 + 10;
  // expOrgN = ...

  // Second fert (5-2-3)
  expLitterC += 2;
  expMinN += 3;
  // expOrgN = ...
  status |= checkOutput(expLitterC, expMinN);

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

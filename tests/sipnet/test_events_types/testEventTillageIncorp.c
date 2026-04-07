#include <stdio.h>
#include <stdlib.h>

#include "utils/tUtils.h"

#include "typesUtils.h"

int checkSoilOutput(double expSoilC, double expLitterC, double expSoilOrgN,
                    double expLitterN) {
  int status = 0;
  if (!compareDoubles(expSoilC, envi.soilC)) {
    logTest("Soil C is %f, expected %f\n", envi.soilC, expSoilC);
    status = 1;
  }
  if (!compareDoubles(expLitterC, envi.litterC)) {
    logTest("Litter C is %f, expected %f\n", envi.litterC, expLitterC);
    status = 1;
  }
  if (!compareDoubles(expSoilOrgN, envi.soilOrgN)) {
    logTest("Soil Org N is %f, expected %f\n", envi.soilOrgN, expSoilOrgN);
    status = 1;
  }
  if (!compareDoubles(expLitterN, envi.litterN)) {
    logTest("Litter N is %f, expected %f\n", envi.litterN, expLitterN);
    status = 1;
  }
  return status;
}

void initEnv(void) {
  envi.soilC = 100.0;
  envi.litterC = 50.0;
  envi.soilOrgN = 5.0;
  envi.litterN = 2.0;
  envi.soilWater = 10.0;
  envi.minN = 10.0;
  envi.plantLeafC = 0.0;
  envi.plantWoodC = 0.0;
  envi.fineRootC = 0.0;
  envi.coarseRootC = 0.0;
  envi.plantWoodCStorageDelta = 0.0;
  initEventTrackers();
}

int run(void) {
  int status = 0;
  double expSoilC, expLitterC;
  double expSoilOrgN, expLitterN;

  prepTypesTest();

  // events_tillage_incorp.in: "2024 70 till 0.3 0.5"
  // tillageEffect=0.3, incorporationFraction=0.5
  logTest("Testing incorporation, litter pool on, N cycle off\n");
  updateIntContext("litterPool", 1, CTX_TEST);
  updateIntContext("nitrogenCycle", 0, CTX_TEST);
  initEnv();
  initEvents("events_tillage_incorp.in", "events.out", 0);
  setupEvents();
  procEvents();
  closeEventOutFile();

  // 50% of 50.0 litterC = 25.0 transferred to soil
  expLitterC = 50.0 - 25.0;
  expSoilC = 100.0 + 25.0;
  expSoilOrgN = 5.0;  // N cycle off, no N transfer
  expLitterN = 2.0;
  status |= checkSoilOutput(expSoilC, expLitterC, expSoilOrgN, expLitterN);

  // also check that tillage decomp modifier was applied
  if (!compareDoubles(eventTrackers.d_till_mod, 0.3)) {
    logTest("d_till_mod is %f, expected 0.3\n", eventTrackers.d_till_mod);
    status |= 1;
  }

  logTest("Testing incorporation with N cycle on\n");
  updateIntContext("nitrogenCycle", 1, CTX_TEST);
  initEnv();
  initEvents("events_tillage_incorp.in", "events.out", 0);
  setupEvents();
  procEvents();
  closeEventOutFile();

  // 50% of 50.0 litterC = 25.0 transferred
  // 50% of 2.0 litterN = 1.0 transferred
  expLitterC = 50.0 - 25.0;
  expSoilC = 100.0 + 25.0;
  expLitterN = 2.0 - 1.0;
  expSoilOrgN = 5.0 + 1.0;
  status |= checkSoilOutput(expSoilC, expLitterC, expSoilOrgN, expLitterN);

  logTest("Testing incorporation with litter pool off\n");
  updateIntContext("litterPool", 0, CTX_TEST);
  updateIntContext("nitrogenCycle", 0, CTX_TEST);
  initEnv();
  initEvents("events_tillage_incorp.in", "events.out", 0);
  setupEvents();
  procEvents();
  closeEventOutFile();

  // no litter pool means no incorporation, pools unchanged
  expSoilC = 100.0;
  expLitterC = 50.0;
  expSoilOrgN = 5.0;
  expLitterN = 2.0;
  status |= checkSoilOutput(expSoilC, expLitterC, expSoilOrgN, expLitterN);

  // decomp modifier should still be set regardless
  if (!compareDoubles(eventTrackers.d_till_mod, 0.3)) {
    logTest("d_till_mod is %f, expected 0.3 (litter pool off)\n",
            eventTrackers.d_till_mod);
    status |= 1;
  }

  // old 1-param format should still work (incorporationFraction defaults to 0)
  logTest("Testing 1-param tillage format (no incorporation)\n");
  updateIntContext("litterPool", 1, CTX_TEST);
  initEnv();
  initEvents("events_one_tillage.in", "events.out", 0);
  setupEvents();
  procEvents();
  closeEventOutFile();

  // no incorporation, pools unchanged except decomp modifier
  expSoilC = 100.0;
  expLitterC = 50.0;
  expSoilOrgN = 5.0;
  expLitterN = 2.0;
  status |= checkSoilOutput(expSoilC, expLitterC, expSoilOrgN, expLitterN);

  if (!compareDoubles(eventTrackers.d_till_mod, 0.5)) {
    logTest("d_till_mod is %f, expected 0.5 (1-param format)\n",
            eventTrackers.d_till_mod);
    status |= 1;
  }

  return status;
}

int main(void) {
  logTest("Starting testEventTillageIncorp\n");
  int status = run();
  if (status) {
    logTest("FAILED testEventTillageIncorp with status %d\n", status);
    exit(status);
  }

  logTest("PASSED testEventTillageIncorp\n");
}

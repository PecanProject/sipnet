#include <stdio.h>
#include <stdlib.h>

#include "utils/tUtils.h"
#include "utils/exitHandler.c"
#include "utils/helpers.c"

int checkLeafOn(const char *stage, double expWoodC, double expCoarseRootC,
                double expLeafC) {
  int status = 0;
  if (!compareDoubles(expWoodC, envi.plantWoodC)) {
    logTest("%s: plantWoodC is %f, expected %f\n", stage, envi.plantWoodC,
            expWoodC);
    status = 1;
  }
  if (!compareDoubles(expCoarseRootC, envi.coarseRootC)) {
    logTest("%s: coarseRootC is %f, expected %f\n", stage, envi.coarseRootC,
            expCoarseRootC);
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

int checkPlantStorageN(const char *stage, double expStorageN) {
  int status = 0;
  if (!compareDoubles(expStorageN, envi.plantStorageN)) {
    logTest("%s: plantStorageN is %f, expected %f\n", stage, envi.plantStorageN,
            expStorageN);
    status = 1;
  }
  return status;
}

void initEnv(double woodC, double leafC) {
  envi.plantWoodC = woodC;
  envi.plantLeafC = leafC;
  envi.coarseRootC = 0.0;
  envi.litterC = 0.0;
  envi.soilC = 0.0;
  envi.litterN = 0.0;
  envi.plantStorageN = 0.0;
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
  params.leafOnReallocFrac = 0.5;

  //// ONE LEAFON EVENT
  logTest("Testing one leafon event\n");
  initEnv(10.0, 0.0);
  resetFluxes();
  initEvents("events_one_leafon.in", "events.out", 0);
  setupEvents();
  procEvents();
  closeEventOutFile();

  // leafOn = params.leafGrowth = 3.0
  // wood decreases by 3.0, leaf increases by 3.0 (coarseRootC=0 so all from
  // wood)
  status |= checkLeafOn("one leafon", 10.0 - 3.0, 0.0, 0.0 + 3.0);

  //// TWO LEAFON EVENTS
  logTest("Testing two leafon events\n");
  initEnv(10.0, 0.0);
  resetFluxes();
  initEvents("events_two_leafon.in", "events.out", 0);
  setupEvents();
  procEvents();
  closeEventOutFile();

  // Two leafOn events: total 2 * 3.0 = 6.0 from wood to leaf (coarseRootC=0)
  status |= checkLeafOn("two leafon", 10.0 - 6.0, 0.0, 0.0 + 6.0);

  //// LEAF-ON WITH PROPORTIONAL WOOD AND COARSE ROOT DRAW
  logTest("Testing leaf-on draws proportionally from wood and coarseRootC\n");
  // woodC=6.0, coarseRootC=4.0: wood fraction = 6/10=0.6, root fraction =
  // 4/10=0.4 leafGrowth=3.0 -> from wood: 3.0*0.6=1.8, from
  // coarseRoot: 3.0*0.4=1.2
  initEnv(6.0, 0.0);
  envi.coarseRootC = 4.0;
  resetFluxes();
  initEvents("events_one_leafon.in", "events.out", 0);
  setupEvents();
  procEvents();
  closeEventOutFile();

  status |=
      checkLeafOn("leafon proportional split", 6.0 - 1.8, 4.0 - 1.2, 0.0 + 3.0);

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

  //// C-LIMITED LEAF-ON EVENT
  logTest("Testing C-limited leaf-on event\n");
  // leafGrowth=3.0, leafOnReallocFrac=0.5, woodC=1.0
  // availableC = woodC * leafOnReallocFrac = 0.5 < leafGrowth=3.0
  // cRatio = 0.5/3.0 -> transferred = leafGrowth * (availableC/leafGrowth) =
  // availableC = 0.5
  updateIntContext("litterPool", 1, CTX_TEST);
  initEnv(1.0, 0.0);
  resetFluxes();
  initEvents("events_one_leafon.in", "events.out", 0);
  setupEvents();
  procEvents();
  closeEventOutFile();

  double availableC = 1.0 * params.leafOnReallocFrac;  // = 0.5
  status |= checkLeafOn("C-limited leafon", 1.0 - availableC, 0.0, availableC);

  //// N-LIMITED LEAF-ON EVENT
  logTest("Testing N-limited leaf-on event (with nitrogen cycle)\n");
  // woodC=10.0 (C not limiting), plantStorageN=0.05 (N limiting)
  // leafOnNDemand = leafGrowth*(1/leafCN - 1/woodCN) = 3*(1/30-1/100) = 0.07
  // nRatio = 0.05/0.07 < cRatio -> transferred = leafGrowth * nRatio
  updateIntContext("anaerobic", 1, CTX_TEST);
  updateIntContext("waterHResp", 1, CTX_TEST);
  updateIntContext("nitrogenCycle", 1, CTX_TEST);
  params.woodCN = 100.0;
  params.nFixationFracMax = 0.0;
  params.halfNFixationMax = 0.0;
  initEnv(10.0, 0.0);
  envi.plantStorageN = 0.05;
  resetFluxes();
  initEvents("events_one_leafon.in", "events.out", 0);
  setupEvents();
  procEvents();
  closeEventOutFile();

  double leafOnNDemand =
      params.leafGrowth / params.leafCN - params.leafGrowth / params.woodCN;
  double nRatio = 0.05 / leafOnNDemand;
  double transferred = params.leafGrowth * nRatio;
  status |=
      checkLeafOn("N-limited leafon", 10.0 - transferred, 0.0, transferred);

  // Reset nitrogen-cycle context for subsequent tests
  updateIntContext("nitrogenCycle", 0, CTX_TEST);
  updateIntContext("anaerobic", 0, CTX_TEST);
  updateIntContext("waterHResp", 0, CTX_TEST);

  //// LEAF-OFF N RESORPTION TEST
  logTest("Testing leaf-off N resorption updates plantStorageN\n");
  // leafOff = 10.0 * 0.5 = 5.0; leafN = 5.0/30.0
  // leafNResorption = leafN * leafNResorptionFrac; litterNAdd = leafN -
  // resorption After event: plantStorageN += leafNResorption, litterN +=
  // litterNAdd
  updateIntContext("litterPool", 1, CTX_TEST);
  updateIntContext("anaerobic", 1, CTX_TEST);
  updateIntContext("waterHResp", 1, CTX_TEST);
  updateIntContext("nitrogenCycle", 1, CTX_TEST);
  params.leafNResorptionFrac = 0.3;
  initEnv(5.0, 10.0);
  resetFluxes();
  initEvents("events_one_leafoff.in", "events.out", 0);
  setupEvents();
  procEvents();
  closeEventOutFile();

  double leafOff = 10.0 * params.fracLeafFall;
  double leafN = leafOff / params.leafCN;
  double leafNResorption = leafN * params.leafNResorptionFrac;
  double litterNAdd = leafN - leafNResorption;
  status |= checkLeafOff("leaf-off N resorption", 10.0 - leafOff, leafOff,
                         litterNAdd);
  status |= checkPlantStorageN("leaf-off N resorption", leafNResorption);

  // Reset for subsequent tests
  params.leafNResorptionFrac = 0.0;
  updateIntContext("nitrogenCycle", 0, CTX_TEST);
  updateIntContext("anaerobic", 0, CTX_TEST);
  updateIntContext("waterHResp", 0, CTX_TEST);

  //// CHECK FOR CALCULATED LEAF EVENTS

  // Restore context for the following tests: no flags set, leafOnDay and
  // leafOffDay set to 0 (disabled), so readEventData should NOT error
  updateIntContext("litterPool", 1, CTX_TEST);
  updateIntContext("gdd", 0, CTX_TEST);
  updateIntContext("soilPhenol", 0, CTX_TEST);
  params.leafOnDay = 0;
  params.leafOffDay = 0;

  int jmp_rval;

  // Baseline: no calculated leaf events configured; readEventData should
  // succeed without calling exit()
  logTest("Testing checkForCalculateLeafEvents: baseline (no conflict)\n");
  should_exit = 0;
  exit_result = 1;
  jmp_rval = setjmp(jump_env);
  if (!jmp_rval) {
    readEventData("events_one_leafon.in");
  }
  test_assert(jmp_rval == 0);
  status |= !exit_result;
  if (!exit_result) {
    logTest("FAIL checkForCalculateLeafEvents baseline\n");
  }

  // ctx.gdd=1 should error
  logTest("Testing checkForCalculateLeafEvents: gdd=1 conflicts\n");
  updateIntContext("gdd", 1, CTX_TEST);
  should_exit = 1;
  exit_result = 1;
  expected_code = EXIT_CODE_BAD_PARAMETER_VALUE;
  jmp_rval = setjmp(jump_env);
  if (!jmp_rval) {
    readEventData("events_one_leafon.in");
  }
  test_assert(jmp_rval == 1);
  status |= !exit_result;
  if (!exit_result) {
    logTest("FAIL checkForCalculateLeafEvents with gdd=1\n");
  }
  updateIntContext("gdd", 0, CTX_TEST);

  // ctx.soilPhenol=1 should error
  logTest("Testing checkForCalculateLeafEvents: soilPhenol=1 conflicts\n");
  updateIntContext("soilPhenol", 1, CTX_TEST);
  should_exit = 1;
  exit_result = 1;
  expected_code = EXIT_CODE_BAD_PARAMETER_VALUE;
  jmp_rval = setjmp(jump_env);
  if (!jmp_rval) {
    readEventData("events_one_leafon.in");
  }
  test_assert(jmp_rval == 1);
  status |= !exit_result;
  if (!exit_result) {
    logTest("FAIL checkForCalculateLeafEvents with soilPhenol=1\n");
  }
  updateIntContext("soilPhenol", 0, CTX_TEST);

  // params.leafOnDay > 0 should error
  logTest("Testing checkForCalculateLeafEvents: leafOnDay>0 conflicts\n");
  params.leafOnDay = 47.0;
  should_exit = 1;
  exit_result = 1;
  expected_code = EXIT_CODE_BAD_PARAMETER_VALUE;
  jmp_rval = setjmp(jump_env);
  if (!jmp_rval) {
    readEventData("events_one_leafon.in");
  }
  test_assert(jmp_rval == 1);
  status |= !exit_result;
  if (!exit_result) {
    logTest("FAIL checkForCalculateLeafEvents with leafOnDay>0\n");
  }
  params.leafOnDay = 0;

  // params.leafOffDay > 0 should error
  logTest("Testing checkForCalculateLeafEvents: leafOffDay>0 conflicts\n");
  params.leafOffDay = 285.0;
  should_exit = 1;
  exit_result = 1;
  expected_code = EXIT_CODE_BAD_PARAMETER_VALUE;
  jmp_rval = setjmp(jump_env);
  if (!jmp_rval) {
    readEventData("events_one_leafoff.in");
  }
  test_assert(jmp_rval == 1);
  status |= !exit_result;
  if (!exit_result) {
    logTest("FAIL checkForCalculateLeafEvents with leafOffDay>0\n");
  }
  params.leafOffDay = 0;

  // Allow real exit for the remainder of the test
  really_exit = 1;

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

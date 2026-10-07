// Native three-step fixture from sipnet-termination-repro.tar.gz, derived from
// wet_d0.1_m1, point 332581. The first step primes mean NPP; positive PAR is
// intentional even at the midnight harvest timestamp. This is a constructed
// model regression fixture, not a field observation or restart replay.
#include "utils/tUtils.h"
#include "sipnet/events.c"
#include "sipnet/sipnet.c"
#include <sys/wait.h>

static int failures;
static ModelParams *testParams;

static void near(double actual, double expected, const char *name) {
  if (!isfinite(actual) || fabs(actual - expected) > 1e-8) {
    logTest("%s: %.15g, expected %.15g\n", name, actual, expected);
    failures++;
  }
}

static void balanced(void) {
  near(balanceTracker.deltaC, 0, "C balance");
  near(balanceTracker.deltaN, 0, "N balance");
  near(balanceTracker.clampedC, 0, "C clamping");
  near(balanceTracker.clampedN, 0, "N clamping");
}

static void processEvents(void) {
  EventNode *event = getCurrentEvent();
  processEventsForCarbon(event);
  processEventsForNitrogen(event);
}

static void start(int mode, const char *harvest) {
  initContext();
  ctx.events = 1;
  ctx.litterPool = mode > 0;
  ctx.nitrogenCycle = mode == 2;
  ctx.anaerobic = 1;
  ctx.waterHResp = 1;
  ctx.growthResp = 0;
  ctx.leafWater = 0;
  ctx.flooding = 1;
  ctx.gdd = 0;
  ctx.soilPhenol = 0;
  envi = (Envi){0};
  fluxes = (Fluxes){0};
  eventTrackers = (EventTrackers){0};
  initModel(&testParams, "termination.param", "termination.clim");
  setupModel();
  gEvents = harvest ? createEventNode(2017, 20, HARVEST, harvest) : NULL;
  setupEvents();
  openEventOutFile("events.out", 1);
  updateState();  // Prime mean NPP with the native first timestep.
  balanced();
  climate = climate->nextClim;
}

static void finish(void) {
  cleanupModel();
  deleteModelParams(testParams);
  gEvents = gEvent = NULL;
}

static void clearPlant(void) {
  near(envi.plantLeafC, 0, "leaf");
  near(envi.plantWoodC, 0, "wood");
  near(envi.fineRootC, 0, "fine roots");
  near(envi.coarseRootC, 0, "coarse roots");
  near(envi.plantCAccountingDelta, 0, "accounting C");
  near(envi.plantStorageN, 0, "storage N");
  near(plantSurvivalTracker.isAlive, 0, "dead");
  near(getMeanTrackerMean(meanNPP), 0, "mean NPP");
}

static void fullCase(int mode, int accounting, int dark, int limited,
                     const char *harvest, double aboveExport,
                     double belowExport) {
  // A no-harvest run supplies the independently calculated end-of-step pools.
  logTest(
      "*** Running full case with mode: %d accounting: %d dark: %d limited: %d"
      " harvest: %s aboveExport: %.2f belowExport: %.2f\n",
      mode, accounting, dark, limited, harvest ? harvest : "none", aboveExport,
      belowExport);
  Envi end = {0};
  for (int h = 0; h < 2; h++) {
    start(mode, h ? harvest : NULL);
    envi.plantCAccountingDelta = accounting;
    if (dark)
      climate->par = 0;
    if (limited && mode == 2) {
      envi.minN = 0;
      envi.plantStorageN = 0;
    }
    updateState();
    balanced();
    if (!h) {
      // First time through, capture envi state
      end = envi;
    } else {
      const double aboveC =
          end.plantLeafC + end.plantWoodC + end.plantCAccountingDelta;
      const double belowC = end.fineRootC + end.coarseRootC;
      const double aboveN = mode == 2 ? end.plantLeafC / params.leafCN +
                                            end.plantWoodC / params.woodCN
                                      : 0;
      const double belowN = mode == 2 ? end.fineRootC / params.fineRootCN +
                                            end.coarseRootC / params.woodCN
                                      : 0;
      near(envi.soilC,
           end.soilC + belowC * (1 - belowExport) +
               (mode == 0 ? aboveC * (1 - aboveExport) : 0),
           "soil routing");
      near(envi.litterC, end.litterC + (mode ? aboveC * (1 - aboveExport) : 0),
           "litter routing");
      near(fluxes.eventOutputC * climate->length,
           aboveC * aboveExport + belowC * belowExport, "C export");
      near(fluxes.eventOutputN * climate->length,
           aboveN * aboveExport + belowN * belowExport, "N export");

      if (mode == 2) {
        near(envi.litterN,
             end.litterN + aboveN * (1 - aboveExport) + end.plantStorageN,
             "litter N including storage");
        near(envi.soilOrgN, end.soilOrgN + belowN * (1 - belowExport),
             "soil N");
      }
      clearPlant();
      climate = climate->nextClim;
      updateState();
      balanced();
      clearPlant();
      near(fluxes.photosynthesis, 0, "no fallow photosynthesis");
      // A later planting can restore living tissue.
      EventNode *plant = createEventNode(2017, 21, PLANTING, "2 3 4 5");
      gEvents->nextEvent = plant;
      gEvent = plant;
      climate->day = 21;
      updateState();
      balanced();
      near(plantSurvivalTracker.isAlive, 1, "alive after planting");
    }
    finish();
  }
}

static void partialCase(int accounting, const char *harvest, double fraction) {
  logTest(
      "*** Running partial case with accounting: %d harvest: %s fraction: %f\n",
      accounting, harvest, fraction);
  start(2, harvest);
  envi.plantCAccountingDelta = accounting;
  Envi before = envi;
  resetFluxes();
  processEvents();
  updateBalanceTrackerPreUpdate();
  updatePoolsForEvents();
  near(envi.plantWoodC, before.plantWoodC * (1 - fraction), "partial wood");
  near(envi.plantCAccountingDelta, accounting * (1 - fraction),
       "partial accounting");
  updateBalanceTrackerPostUpdate();
  near(balanceTracker.postTotalC + fluxes.eventOutputC * climate->length,
       balanceTracker.preTotalC, "partial C conservation");
  near(balanceTracker.postTotalN + fluxes.eventOutputN * climate->length,
       balanceTracker.preTotalN, "partial N conservation");
  finish();
}

static void partialTimestepCase(int accounting) {
  logTest("*** Running partial timestep case\n");
  start(2, ".1 .2 .3 .4");
  envi.plantCAccountingDelta = accounting;
  updateState();
  balanced();
  near(plantSurvivalTracker.isAlive, 1, "partial harvest remains alive");
  climate = climate->nextClim;
  updateState();
  balanced();
  near(plantSurvivalTracker.isAlive, 1, "partial harvest can continue growth");
  finish();
}

static void runPartialHarvestLeafBudgetCase(const char *name, double meanNPPValue,
                                            double leafTurnoverRate,
                                            double harvestFraction) {
  char harvest[32];
  snprintf(harvest, sizeof(harvest), "%.8f 0 %.8f 0", harvestFraction / 2,
           harvestFraction / 2);
  start(1, harvest);

  envi.plantLeafC = 1.0;
  envi.plantWoodC = envi.fineRootC = envi.coarseRootC = 100.0;
  climate->par = 0;
  resetMeanTracker(meanNPP, meanNPPValue);
  params.leafAllocation = params.woodAllocation =
      params.fineRootAllocation = params.coarseRootAllocation = 0.25;
  params.leafTurnoverRate = leafTurnoverRate;
  params.woodTurnoverRate = params.fineRootTurnoverRate =
      params.coarseRootTurnoverRate = 0;
  params.baseFolRespFrac = params.baseVegResp = params.baseSoilResp = 0;
  params.baseFineRootResp = params.baseCoarseRootResp = 0;
  params.litterBreakdownRate = params.soilMethaneRate =
      params.litterMethaneRate = 0;
  params.leafGrowth = 366;
  params.leafOffDay = 366;

  updateState();
  balanced();
  if (envi.plantLeafC < 0) {
    logTest("%s left a negative leaf pool: %.15g\n", name, envi.plantLeafC);
    failures++;
  }
  finish();
}

static void partialHarvestLeafBudgetCase(const char *name, double meanNPPValue,
                                         double leafTurnoverRate,
                                         double harvestFraction) {
  logTest("*** Running partial harvest leaf budget case: %s\n", name);
  fflush(NULL);
  pid_t child = fork();
  if (child < 0) {
    failures++;
    return;
  }
  if (child == 0) {
    int failuresBefore = failures;
    runPartialHarvestLeafBudgetCase(name, meanNPPValue, leafTurnoverRate,
                                    harvestFraction);
    fflush(NULL);
    _exit(failures == failuresBefore ? EXIT_SUCCESS : EXIT_FAILURE);
  }

  int status;
  if (waitpid(child, &status, 0) != child) {
    logTest("%s partial harvest leaf budget case wait failed\n", name);
    failures++;
  } else if (!WIFEXITED(status) || WEXITSTATUS(status) != EXIT_SUCCESS) {
    logTest("%s partial harvest leaf budget case failed (status %d)\n", name,
            WIFEXITED(status) ? WEXITSTATUS(status) : -1);
    failures++;
  }
}

static void invalidCase(int which) {
  pid_t child = fork();
  if (child < 0) {
    failures++;
    return;
  }
  if (child == 0) {
    start(2, "0 0 1 1");
    if (which < 4) {  // 0, 1, 2, 3
      const char *bad[] = {"0 0 -1 1", "-0.1 0 1 1", "0 0 1.1 1", "1 0.1 0 1"};
      // createEventNode should reject these
      createEventNode(2017, 20, HARVEST, bad[which]);
    } else {  // 4, 5, 6
      EventNode *extra = createEventNode(2017, 20, HARVEST, "0 0 .2 .2");
      if (which == 4) {
        gEvents->nextEvent = extra;
        // Harvest greater than 100%
        processEvents();
      } else if (which == 5) {
        extra->nextEvent = gEvents;
        gEvents = extra;
        setupEvents();
        // Harvest greater than 100%
        processEvents();
      } else {  // 6
        EventNode *leafOff1 = createEventNode(2017, 20, LEAFOFF, "");
        EventNode *leafOff2 = createEventNode(2017, 20, LEAFOFF, "");
        leafOff1->nextEvent = leafOff2;
        gEvents = leafOff1;
        setupEvents();
        processEvents();
      }
    }
    _exit(99);
  }
  int status;
  waitpid(child, &status, 0);
  int expected = EXIT_CODE_BAD_PARAMETER_VALUE;
  if (!WIFEXITED(status) || WEXITSTATUS(status) != expected) {
    logTest("Invalid case %d did not exit with code %d (was %d)\n", which,
            expected, WIFEXITED(status) ? WEXITSTATUS(status) : -1);
    failures++;
  }
}

static void coincidentFertilizerCase(void) {
  logTest("*** Running coincident fertilizer case\n");
  Envi expected = {0};
  for (int h = 0; h < 2; h++) {
    start(2, h ? "0 0 1 1" : NULL);
    EventNode *fert = createEventNode(2017, 20, FERTILIZATION, "1 2 3");
    fert->nextEvent = gEvents;
    gEvents = fert;
    setupEvents();
    updateState();
    balanced();
    if (!h)
      expected = envi;
    else {
      near(envi.soilC,
           expected.soilC + expected.fineRootC + expected.coarseRootC,
           "fertilizer plus harvest soil C");
      near(envi.litterC,
           expected.litterC + expected.plantWoodC + expected.plantLeafC +
               expected.plantCAccountingDelta,
           "fertilizer plus harvest litter C");
      near(envi.minN, expected.minN, "fertilizer applied once");
      clearPlant();
    }
    finish();
  }
}

static void sameEnvi(const Envi *a, const Envi *b) {
#define E(f) near(a->f, b->f, #f)
  E(plantWoodC);
  E(plantLeafC);
  E(soilC);
  E(soilWater);
  E(litterC);
  E(snow);
  E(coarseRootC);
  E(fineRootC);
  E(minN);
  E(soilOrgN);
  E(litterN);
  E(plantStorageN);
  E(plantCAccountingDelta);
#undef E
}
static void coincidentEventCase(int type, const char *arguments) {
  logTest("*** Running coincident event case with type: %s params: %s\n",
          eventTypeToString(type), *arguments ? arguments : "<none>");
  Envi end;
  Fluxes ordinary;
  for (int order = 0; order < 3; order++) {
    start(2, order ? ".25 .75 .75 .25" : NULL);
    params.leafGrowth = 1;
    params.fracLeafFall = .25;
    EventNode *extra = createEventNode(2017, 20, type, arguments);
    if (order == 2)
      gEvents->nextEvent = extra;
    else {
      extra->nextEvent = gEvents;
      gEvents = extra;
    }
    setupEvents();
    updateState();
    balanced();
    if (!order) {
      end = envi;
      ordinary = fluxes;
      if ((type == LEAFON && fluxes.eventLeafOnCreation <= 0) ||
          (type == LEAFOFF && fluxes.eventLeafOffLitterC <= 0) ||
          (type == IRRIGATION && fluxes.eventSoilWater <= 0))
        failures++;
    } else {
      double dt = climate->length;
      double ac = end.plantWoodC + end.plantCAccountingDelta + end.plantLeafC;
      double bc = end.fineRootC + end.coarseRootC;
      double an =
          end.plantWoodC / params.woodCN + end.plantLeafC / params.leafCN;
      double bn =
          end.fineRootC / params.fineRootCN + end.coarseRootC / params.woodCN;
      Envi expected = end;
      expected.soilC += .25 * bc;
      expected.litterC += .75 * ac;
      expected.soilOrgN += .25 * bn;
      expected.litterN += .75 * an + end.plantStorageN;
      expected.plantWoodC = expected.plantLeafC = expected.fineRootC = 0;
      expected.coarseRootC = expected.plantCAccountingDelta =
          expected.plantStorageN = 0;
      sameEnvi(&envi, &expected);
      clearPlant();
      near(fluxes.eventInputC, ordinary.eventInputC, "C input once");
      near(fluxes.eventInputN, ordinary.eventInputN, "N input once");
      near(fluxes.eventOutputC,
           ordinary.eventOutputC + (.25 * ac + .75 * bc) / dt, "C export");
      near(fluxes.eventOutputN,
           ordinary.eventOutputN + (.25 * an + .75 * bn) / dt, "N export");
#define F(f) near(fluxes.f, ordinary.f, #f)
      F(eventSoilWater);
      F(eventEvap);
      F(eventLeafOnCreation);
      F(eventLeafOnCreationFromWood);
      F(eventLeafOffLitterC);
      F(eventLeafOffNResorption);
#undef F
      climate = climate->nextClim;
      updateState();
      balanced();
      clearPlant();
      near(fluxes.photosynthesis, 0, "no fallow photosynthesis");
    }
    finish();
  }
}

static void restartCase(void) {
  logTest("*** Running restart case\n");
  Envi expected;
  for (int resumed = 0; resumed < 2; resumed++) {
    start(2, "0 0 1 1");
    if (resumed) {
      restartNoteProcessedClimateStep(firstClimate);
      restartWriteCheckpoint("termination.restart", meanNPP);
      envi = (Envi){0};
      resetMeanTracker(meanNPP, 0);
      restartLoadCheckpoint("termination.restart", meanNPP);
    }
    updateState();
    balanced();
    clearPlant();
    if (resumed) {
      restartNoteProcessedClimateStep(climate);
      restartWriteCheckpoint("termination.restart", meanNPP);
    }
    climate = climate->nextClim;
    if (resumed) {
      envi = (Envi){0};
      restartLoadCheckpoint("termination.restart", meanNPP);
    }
    updateState();
    balanced();
    clearPlant();
    if (!resumed)
      expected = envi;
    else {
      sameEnvi(&envi, &expected);
      near(envi.soilC, expected.soilC, "restart soil C");
      near(envi.litterC, expected.litterC, "restart litter C");
      near(envi.minN, expected.minN, "restart mineral N");
      near(envi.soilOrgN, expected.soilOrgN, "restart soil N");
      near(envi.litterN, expected.litterN, "restart litter N");
    }
    finish();
  }
}

static void leafBudgetCases(void) {
  int caseNum = 0;
  char harvests[4][16] = {"none", "0 0 1 1", "0.5 0.5 0.5 0.5", "1 1 0 0"};
  for (int kind = 0; kind < 4; kind++) {
    // kind:          0    1    2    3
    // fracLeafFall   1  .25  .40  .75
    // leafOff        a    a    b    c
    // a: inserted at start
    // b: two events inserted at start
    // c: no leaf-off events
    for (int dark = 0; dark < 2; dark++) {
      // dark : set climate->par=0 when dark
      for (int sign = -1; sign <= 1; sign++) {
        // sign: init npp tracker with 20*sign (-20, 0, 20)
        for (int account = -1; account <= 1; account++) {
          // account: starting value for plantCAccountingDelta
          for (int limited = 0; limited < 2; limited++) {
            // limited: starting minN = 0 if limited (1000 else)
            for (int resorb = 0; resorb < 3; resorb++) {
              /// resorb: leafNResorptionFrac = 0, .5, 1  (resorb/2)
              for (int harvest = 0; harvest < 4; harvest++) {
                char *harvestStr = harvests[harvest];
                logTest(
                    "*** Running leaf budget case [%d] with kind: %d dark: %d "
                    "sign %d account %d limited %d resorb %.1f harvest %s\n",
                    caseNum++, kind, dark, sign, account, limited, resorb / 2.0,
                    harvestStr);
                start(2, harvest ? harvestStr : NULL);
                envi.plantLeafC = 1;
                envi.plantWoodC = envi.fineRootC = envi.coarseRootC = 100;
                envi.plantCAccountingDelta = account;
                envi.plantStorageN = 0;
                envi.minN = limited ? 0 : 1000;
                params.leafAllocation = params.woodAllocation = .25;
                params.fineRootAllocation = params.coarseRootAllocation = .25;
                params.leafTurnoverRate = 2;
                params.woodTurnoverRate = params.fineRootTurnoverRate = 0;
                params.coarseRootTurnoverRate = params.litterBreakdownRate = 0;
                params.baseSoilResp = params.soilMethaneRate =
                    params.litterMethaneRate = 0;
                params.nVolatilizationFrac = params.nLeachingFrac = 0;
                params.nFixationFracMax = 0;
                params.leafNResorptionFrac = .5 * resorb;
                params.fracLeafFall = kind == 0   ? 1
                                      : kind == 1 ? .25
                                      : kind == 2 ? .40
                                                  : .75;
                if (dark)
                  climate->par = 0;
                resetMeanTracker(meanNPP, 20 * sign);
                if (kind != 3) {
                  EventNode *off = createEventNode(2017, 20, LEAFOFF, "");
                  off->nextEvent = gEvents;
                  gEvents = off;
                  if (kind == 2) {
                    EventNode *second = createEventNode(2017, 20, LEAFOFF, "");
                    second->nextEvent = off->nextEvent;
                    off->nextEvent = second;
                  }
                  setupEvents();
                }
                double beforeLeafC = envi.plantLeafC;
                double beforeLitter = envi.litterC;
                double beforeLitterN = envi.litterN;
                updateState();
                balanced();
                double shed = (fluxes.leafLitter + fluxes.leafOffLitter +
                               fluxes.eventLeafOffLitterC) *
                              climate->length;
                double pool = 1 + fluxes.leafCreation * climate->length;
                if (pool - shed < -1e-10 || envi.plantLeafC < -1e-10) {
                  logTest("Leaf budget overdraw (2): shed %.17g pool %.17g "
                          "|s-p| %.17g leaf %.17g expr1 %d expr2 %d\n",
                          shed, pool, pool - shed, envi.plantLeafC,
                          pool - shed < -1e-10, envi.plantLeafC < -1e-10);
                  failures++;
                }

                double expLeafOffLitter =
                    kind == 3 ? 0
                              : fmin(params.fracLeafFall * (1 + (kind == 2)),
                                     beforeLeafC + (fluxes.leafCreation -
                                                    fluxes.leafLitter) *
                                                       climate->length);
                near(fluxes.eventLeafOffLitterC * climate->length,
                     expLeafOffLitter, "event leaves conserved");
                if (!harvest) {
                  near(envi.litterC - beforeLitter, shed,
                       "leaf litter transfer");
                  near(envi.litterN - beforeLitterN,
                       shed * (1 - params.leafNResorptionFrac) / params.leafCN,
                       "leaf litter N transfer");
                } else {
                  clearPlant();
                  climate = climate->nextClim;
                  updateState();
                  balanced();
                  clearPlant();
                  near(fluxes.photosynthesis, 0,
                       "no regrowth after complete harvest");
                }
                finish();
              }  // harvest loop
            }  // resorb loop
          }  // limited loop
        }  // account loop
      }  // sign loop
    }  // dark loop
  }  // kind loop
}

// Full export during net loss must not borrow carbon from an empty soil pool.
static void exportLossCase(void) {
  start(1, "1 1 0 0");
  envi.litterC = envi.soilC = envi.litterN = envi.soilOrgN = 0;
  params.baseSoilResp = params.litterBreakdownRate = 0;
  params.soilMethaneRate = params.litterMethaneRate = 0;
  climate->par = 0;
  resetMeanTracker(meanNPP, -20);
  updateState();
  balanced();
  clearPlant();
  finish();
}

int main(void) {

  logTest("Starting testCompleteHarvest\n");

  const char *harvest[] = {"0 0 1 1", "1 1 0 0", ".25 .75 .75 .25"};
  const double above[] = {0, 1, .25}, below[] = {0, 1, .75};
  for (int mode = 0; mode < 3; mode++)
    for (int account = -1; account <= 1; account++)
      for (int dark = 0; dark < 2; dark++)
        for (int limited = 0; limited <= (mode == 2); limited++)
          for (int route = 0; route < 3; route++)
            fullCase(mode, account, dark, limited, harvest[route], above[route],
                     below[route]);

  logTest("\n");
  for (int account = -1; account <= 1; account++) {
    partialTimestepCase(account);
    partialCase(account, ".1 .2 .3 .4", .4);
    partialCase(account, "0 0 .999999999 .999999999", .999999999);
  }

  partialHarvestLeafBudgetCase("negative creation", -4, 0, .9);
  partialHarvestLeafBudgetCase("turnover", 0, 2, .8);
  partialHarvestLeafBudgetCase("negative creation and turnover", -4, 1.5,
                               .7);

  logTest("\n");
  coincidentFertilizerCase();
  coincidentEventCase(LEAFON, "");
  coincidentEventCase(LEAFOFF, "");
  coincidentEventCase(IRRIGATION, "2 0");
  restartCase();

  logTest("\n");

  leafBudgetCases();
  exportLossCase();

  // Leave these last, as the forking messes up the output otherwise
  logTest("\n");
  logTest("*** Running invalid case checks; seven errors expected\n");
  for (int which = 0; which < 7; which++) {
    invalidCase(which);
  }

  logTest("\n");
  logTest("Complete harvest total failures: %d\n", failures);
  logTest("\n");

  int status = failures > 0;

  if (status) {
    logTest("FAILED testCompleteHarvest with status %d\n", status);
    exit(status);
  }

  logTest("PASSED testCompleteHarvest\n");

  return failures != 0;
}

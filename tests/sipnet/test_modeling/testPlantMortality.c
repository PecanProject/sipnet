#include "utils/tUtils.h"
#include "sipnet/events.c"
#include "sipnet/sipnet.c"

/////
// Setup and state management

void resetContext(void) {
  ctx.litterPool = 0;
  ctx.nitrogenCycle = 0;
  ctx.events = 0;
  // waterHResp is on by default, no tests turn it off
  ctx.anaerobic = 0;
}

void setupTests(void) {
  // Set up dummy climate
  climate = (ClimateNode *)malloc(sizeof(ClimateNode));
  climate->year = 2024;
  climate->day = 70;
  climate->time = 0.0;
  climate->length = 0.125;

  // Set up the context
  initContext();
  resetContext();

  // Initialize mean NPP tracker (needed by checkForMortality ->
  // resetMeanTracker)
  meanNPP = newMeanTracker(0, MEAN_NPP_DAYS, MEAN_NPP_MAX_ENTRIES);

  // Initialize event trackers
  initEventTrackers();
}

void resetEnv(void) {
  envi.plantWoodC = 5.0;
  envi.plantLeafC = 2.0;
  envi.plantCAccountingDelta = 0.0;
  envi.fineRootC = 3.0;
  envi.coarseRootC = 4.0;
  envi.soilC = 10.0;
  envi.litterC = 0.0;
  envi.soilOrgN = 0.0;
  envi.litterN = 0.0;
  envi.plantStorageN = 0.0;
  eventTrackers.harvestFracRemoved = 0.0;
  eventTrackers.harvestFracTransferred = 0.0;
}

int checkPool(double calc, double exp, const char *label) {
  validateContext();
  int status = 0;
  if (!compareDoubles(calc, exp)) {
    status = 1;
    logTest("Pool %s is %f, expected %f\n", label, calc, exp);
  }
  return status;
}

/////
// initPlantSurvivalTracker tests

int testInitPlantSurvivalTrackerAlive(void) {
  int status = 0;
  logTest("Running testInitPlantSurvivalTrackerAlive\n");

  resetContext();
  resetEnv();  // woodC=5.0, fineRootC=3.0, coarseRootC=4.0
  initPlantSurvivalTracker();
  if (!plantSurvivalTracker.isAlive) {
    status = 1;
    logTest("Expected isAlive=1 with woodC=5 and roots=7, got 0\n");
  }
  return status;
}

int testInitPlantSurvivalTrackerDeadNoWood(void) {
  int status = 0;
  logTest("Running testInitPlantSurvivalTrackerDeadNoWood\n");

  resetContext();
  resetEnv();
  envi.plantWoodC = 0.0;
  initPlantSurvivalTracker();
  if (plantSurvivalTracker.isAlive) {
    status = 1;
    logTest("Expected isAlive=0 with woodC=0, got 1\n");
  }
  return status;
}

int testInitPlantSurvivalTrackerDeadNoRoots(void) {
  int status = 0;
  logTest("Running testInitPlantSurvivalTrackerDeadNoRoots\n");

  resetContext();
  resetEnv();
  envi.fineRootC = 0.0;
  envi.coarseRootC = 0.0;
  initPlantSurvivalTracker();
  if (plantSurvivalTracker.isAlive) {
    status = 1;
    logTest("Expected isAlive=0 with totalRoots=0, got 1\n");
  }
  return status;
}

int testInitPlantSurvivalTrackerDeadNegativeTotalWood(void) {
  int status = 0;
  logTest("Running testInitPlantSurvivalTrackerDeadNegativeTotalWood\n");

  resetContext();
  resetEnv();
  envi.plantWoodC = 1.0;
  envi.plantCAccountingDelta = -1.5;
  initPlantSurvivalTracker();
  if (plantSurvivalTracker.isAlive) {
    status = 1;
    logTest("Expected isAlive=0 with totalWoodC=-0.5, got 1\n");
  }
  return status;
}

/////
// checkForMortality tests

int testMortalityAliveStaysAlive(void) {
  int status = 0;
  logTest("Running testMortalityAliveStaysAlive\n");

  resetContext();
  resetEnv();  // woodC=5, leafC=2, fineRoot=3, coarseRoot=4
  plantSurvivalTracker.isAlive = 1;

  checkForMortality();

  if (!plantSurvivalTracker.isAlive) {
    status = 1;
    logTest("Expected plant to remain alive\n");
  }
  status |= checkPool(envi.plantWoodC, 5.0, "plantWoodC unchanged");
  status |= checkPool(envi.soilC, 10.0, "soilC unchanged");
  return status;
}

int testMortalityPlantDiesNoLitter(void) {
  int status = 0;
  logTest("Running testMortalityPlantDiesNoLitter\n");

  resetContext();
  resetEnv();
  envi.plantWoodC = 0.0;  // woodC=0 triggers death
  plantSurvivalTracker.isAlive = 1;

  // All pools go to soilC when no litter pool:
  // soilC += fineRoot + coarseRoot = 3 + 4 = 7
  // soilC += woodC + leafC + delta = 0 + 2 + 0 = 2
  double expSoilC = 10.0 + 3.0 + 4.0 + 0.0 + 2.0 + 0.0;
  checkForMortality();

  if (plantSurvivalTracker.isAlive) {
    status = 1;
    logTest("Expected plant to be marked dead\n");
  }
  status |= checkPool(envi.soilC, expSoilC, "soilC after death (no litter)");
  status |= checkPool(envi.plantWoodC, 0.0, "plantWoodC zeroed");
  status |= checkPool(envi.plantLeafC, 0.0, "plantLeafC zeroed");
  status |= checkPool(envi.fineRootC, 0.0, "fineRootC zeroed");
  status |= checkPool(envi.coarseRootC, 0.0, "coarseRootC zeroed");
  status |= checkPool(envi.plantCAccountingDelta, 0.0,
                      "plantCAccountingDelta zeroed");
  return status;
}

int testMortalityPlantDiesWithLitter(void) {
  int status = 0;
  logTest("Running testMortalityPlantDiesWithLitter\n");

  resetContext();
  ctx.litterPool = 1;
  resetEnv();
  envi.litterC = 5.0;
  envi.plantWoodC = 0.0;  // trigger death
  plantSurvivalTracker.isAlive = 1;

  // Roots go to soilC: 10 + 3 + 4 = 17
  // Above-ground goes to litterC: 5 + 0 (woodC) + 2 (leafC) + 0 (delta) = 7
  double expSoilC = 10.0 + 3.0 + 4.0;
  double expLitterC = 5.0 + 0.0 + 2.0 + 0.0;
  checkForMortality();

  status |= checkPool(envi.soilC, expSoilC, "soilC after death (with litter)");
  status |=
      checkPool(envi.litterC, expLitterC, "litterC after death (with litter)");
  status |= checkPool(envi.plantWoodC, 0.0, "plantWoodC zeroed");
  status |= checkPool(envi.plantLeafC, 0.0, "plantLeafC zeroed");
  return status;
}

int testMortalityPlantDiesWithNitrogen(void) {
  int status = 0;
  logTest("Running testMortalityPlantDiesWithNitrogen\n");

  // Nitrogen cycle implies litter pool; set all required context flags
  resetContext();
  ctx.litterPool = 1;
  ctx.anaerobic = 1;
  ctx.nitrogenCycle = 1;

  resetEnv();
  envi.litterC = 5.0;
  envi.soilOrgN = 2.0;
  envi.litterN = 3.0;
  envi.plantStorageN = 0.5;
  params.woodCN = 100.0;
  params.leafCN = 20.0;
  params.fineRootCN = 40.0;
  envi.fineRootC = 3.0;
  envi.coarseRootC = 4.0;
  envi.plantLeafC = 2.0;
  envi.plantWoodC = 0.0;  // trigger death
  plantSurvivalTracker.isAlive = 1;

  // soilOrgN += fineRoot/fineRootCN + coarseRoot/woodCN = 3/40 + 4/100 = 0.075
  // + 0.04 = 0.115 litterN += woodC/woodCN + leafC/leafCN + storageN = 0/100 +
  // 2/20 + 0.5 = 0.6
  double expSoilOrgN = 2.0 + 3.0 / 40.0 + 4.0 / 100.0;
  double expLitterN = 3.0 + 0.0 / 100.0 + 2.0 / 20.0 + 0.5;
  checkForMortality();

  status |= checkPool(envi.soilOrgN, expSoilOrgN, "soilOrgN after death");
  status |= checkPool(envi.litterN, expLitterN, "litterN after death");
  status |= checkPool(envi.plantStorageN, 0.0, "plantStorageN zeroed");
  return status;
}

int testMortalityDeadStaysDead(void) {
  int status = 0;
  logTest("Running testMortalityDeadStaysDead\n");

  resetContext();
  resetEnv();
  // All pools are zero: plant is dead and stays dead
  envi.plantWoodC = 0.0;
  envi.plantLeafC = 0.0;
  envi.fineRootC = 0.0;
  envi.coarseRootC = 0.0;
  plantSurvivalTracker.isAlive = 0;

  checkForMortality();

  if (plantSurvivalTracker.isAlive) {
    status = 1;
    logTest("Expected plant to remain dead\n");
  }
  // soilC should be unchanged (no double-counting)
  status |= checkPool(envi.soilC, 10.0, "soilC unchanged for dead plant");
  return status;
}

int testMortalityReemergence(void) {
  int status = 0;
  logTest("Running testMortalityReemergence\n");

  resetContext();
  resetEnv();  // woodC=5, roots=7
  plantSurvivalTracker.isAlive = 0;  // plant was previously dead

  // After a planting event, woodC and roots are positive -> plant re-emerges
  checkForMortality();

  if (!plantSurvivalTracker.isAlive) {
    status = 1;
    logTest("Expected plant to re-emerge when woodC>0 and rootC>0\n");
  }
  // Pools should be unchanged on re-emergence (no cleanup needed)
  status |=
      checkPool(envi.plantWoodC, 5.0, "plantWoodC unchanged on reemergence");
  status |= checkPool(envi.soilC, 10.0, "soilC unchanged on reemergence");
  return status;
}

int testMortalityWithAccountingDelta(void) {
  int status = 0;
  logTest("Running testMortalityWithAccountingDelta\n");

  resetContext();
  resetEnv();
  // plantWoodC=1 but plantCAccountingDelta=-1.5 -> totalWoodC=-0.5 -> death
  envi.plantWoodC = 1.0;
  envi.plantCAccountingDelta = -1.5;
  plantSurvivalTracker.isAlive = 1;

  // soilC starts at 10, then gets fineRootC=3, coarseRootC=4, plantWoodC=1,
  // plantLeafC=2, and plantCAccountingDelta=-1.5 for a final value of 18.5.
  double expSoilC = 10.0 + 3.0 + 4.0 + 1.0 + 2.0 - 1.5;
  checkForMortality();

  if (plantSurvivalTracker.isAlive) {
    status = 1;
    logTest("Expected plant to die when totalWoodC=-0.5\n");
  }
  status |= checkPool(envi.soilC, expSoilC,
                      "soilC after death (negative totalWoodC)");
  return status;
}

int run(void) {
  int status = 0;

  setupTests();

  status |= testInitPlantSurvivalTrackerAlive();
  status |= testInitPlantSurvivalTrackerDeadNoWood();
  status |= testInitPlantSurvivalTrackerDeadNoRoots();
  status |= testInitPlantSurvivalTrackerDeadNegativeTotalWood();
  status |= testMortalityAliveStaysAlive();
  status |= testMortalityPlantDiesNoLitter();
  status |= testMortalityPlantDiesWithLitter();
  status |= testMortalityPlantDiesWithNitrogen();
  status |= testMortalityDeadStaysDead();
  status |= testMortalityReemergence();
  status |= testMortalityWithAccountingDelta();

  return status;
}

int main(void) {
  int status;

  logTest("Starting testPlantMortality:run()\n");
  status = run();
  if (status) {
    logTest("FAILED testPlantMortality with status %d\n", status);
    exit(status);
  }

  logTest("PASSED testPlantMortality\n");
}

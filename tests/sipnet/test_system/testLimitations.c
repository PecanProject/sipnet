#include "utils/failureTracker.c"
#include "utils/tUtils.h"
#include "sipnet/events.c"
#include "sipnet/sipnet.c"

static int cases;

static void addEvent(int type, const char *arguments) {
  EventNode *event =
      createEventNode(climate->year, climate->day, type, arguments);
  event->nextEvent = gEvents;
  gEvents = event;
}

// Keep ordinary turnover, respiration, and weather inputs at zero so the
// expected pools can be calculated independently of the production fluxes.
// A partial harvest leaves enough biomass for all leaf events and negative
// growth, avoiding the separate leaf-budget and mortality limitations.
static void limitationCase(int harvest, int leafOn, int leafOff, int fertilizer,
                           int sign, int storage, int mineral, int flooded,
                           double lossRate) {
  logTest("*** Limitation case %d: harvest %d leafOn %d leafOff %d fertilizer "
          "%d sign %d storage %d mineral %d flooded %d lossRate %g\n",
          ++cases, harvest, leafOn, leafOff, fertilizer, sign, storage, mineral,
          flooded, lossRate);
  checkpointFailureTracking();
  initContext();
  ctx.events = ctx.litterPool = ctx.nitrogenCycle = ctx.anaerobic = 1;
  ctx.waterHResp = ctx.flooding = 1;
  ctx.growthResp = ctx.leafWater = ctx.gdd = ctx.soilPhenol = 0;
  ctx.carbonSaturation = 0;
  envi = (Envi){0};
  fluxes = (Fluxes){0};
  eventTrackers = (EventTrackers){0};
  ModelParams *testParams;
  initModel(&testParams, "termination.param", "termination.clim");
  setupModel();

  envi.plantLeafC = 10;
  envi.plantWoodC = envi.fineRootC = envi.coarseRootC = 100;
  envi.plantCAccountingDelta = 0;
  envi.litterC = envi.soilC = 100;
  envi.litterN = envi.soilOrgN = 2;
  envi.plantStorageN = storage ? 0.1 : 0;
  envi.minN = mineral ? 1 : 0;
  envi.soilWater = flooded ? 20 : 8;
  envi.snow = 0;

  params.leafCN = 20;
  params.woodCN = 100;
  params.fineRootCN = 40;
  params.leafAllocation = params.woodAllocation = 0.25;
  params.fineRootAllocation = params.coarseRootAllocation = 0.25;
  params.leafTurnoverRate = params.woodTurnoverRate = 0;
  params.fineRootTurnoverRate = params.coarseRootTurnoverRate = 0;
  params.baseFolRespFrac = params.baseVegResp = params.baseSoilResp = 0;
  params.baseFineRootResp = params.baseCoarseRootResp = 0;
  params.litterBreakdownRate = params.soilMethaneRate = 0;
  params.litterMethaneRate = 0;
  params.leafOnDay = params.leafOffDay = 366;
  params.leafGrowth = 1;
  params.leafOnReallocFrac = 0.5;
  params.fracLeafFall = 0.1;
  params.leafNResorptionFrac = 0.5;
  params.soilWHC = 10;
  params.waterDrainFrac = 1;
  params.fAnoxia = 0.6;
  params.soilRespQ10 = 2;
  params.nVolatilizationFrac = params.nLeachingFrac = lossRate;
  params.nFixationFracMax = 0.25;
  params.halfNFixationMax = 1;
  climate->par = climate->precip = climate->vpd = climate->vpdSoil = 0;
  climate->tsoil = 20;
  resetMeanTracker(meanNPP, sign * 20);

  gEvents = gEvent = NULL;
  if (harvest)
    addEvent(HARVEST, ".1 .2 .1 .1");
  for (int i = 0; i < leafOn; i++)
    addEvent(LEAFON, "");
  for (int i = 0; i < leafOff; i++)
    addEvent(LEAFOFF, "");
  if (fertilizer)
    addEvent(FERTILIZATION, "2 3 .5");  // organic N, organic C, mineral N
  setupEvents();
  openEventOutFile("events.out", 0);

  const double len = climate->length;
  const double initialMinN = envi.minN;
  const double initialStorageN = envi.plantStorageN;
  // At W=8, WHC=10, fAnoxia=.6, A=.5 gives moisture effect 1.
  // At W=20 the anaerobic index is 1, giving moisture effect .05.
  const double drainage = flooded ? 10 : 0;
  double volatilization = initialMinN * lossRate * 4 * (flooded ? .05 : 1);
  double leaching = initialMinN * lossRate * (flooded ? 1 : 0);
  const double mineralSupply = initialMinN + (fertilizer ? .5 : 0);
  const double losses = (volatilization + leaching) * len;
  if (losses > mineralSupply) {
    volatilization *= mineralSupply / losses;
    leaching *= mineralSupply / losses;
  }

  // Leaf-on uses storage N only. Each event transfers one unit of C from
  // wood/coarse roots and needs 1/20 - 1/100 = .04 additional N.
  const double newLeaves = storage ? leafOn : 0;
  const double shedLeaves = leafOff;  // 10 * .1 per event
  const double unclaimedStorage = initialStorageN - newLeaves * .04;
  const double demand = fmax(0, sign * 5 * len * (.05 + .01 + .025 + .01));
  const double fixationFraction = .25 / (1 + initialMinN);
  // Fertilizer enters the mineral pool on this step, but is available to
  // growth on subsequent steps (calcMinNNonUptakeFluxes excludes eventMinN).
  const double availableMinN = initialMinN - (volatilization + leaching) * len;
  const double potentialUptake =
      fmax(0, demand - unclaimedStorage) * (1 - fixationFraction);
  double growthFactor = 1;
  if (potentialUptake > 1e-10 && potentialUptake > availableMinN)
    growthFactor =
        (availableMinN / (1 - fixationFraction) + unclaimedStorage) / demand;
  const double growth = sign * 5 * len * growthFactor;
  const double uptake = fmax(0, demand * growthFactor - unclaimedStorage) *
                        (1 - fixationFraction);
  const double fixation = uptake * fixationFraction / (1 - fixationFraction);

  updateState();
  near(balanceTracker.deltaC, 0, "C balance");
  near(balanceTracker.deltaN, 0, "N balance");
  near(balanceTracker.clampedC, 0, "C clamping");
  near(balanceTracker.clampedN, 0, "N clamping");
  near(envi.plantLeafC, 10 - 2 * harvest + newLeaves - shedLeaves + growth,
       "plantLeafC");
  near(envi.litterC, 100 + 11 * harvest + shedLeaves + 3 * fertilizer,
       "litterC");
  near(envi.litterN, 2 + .15 * harvest + shedLeaves * .025 + 2 * fertilizer,
       "litterN");
  near(envi.minN, mineralSupply - (volatilization + leaching) * len - uptake,
       "minN");
  near(envi.soilC, 100 + 20 * harvest, "soilC");
  near(envi.soilOrgN, 2 + .35 * harvest, "soilOrgN");
  near(envi.soilWater, (flooded ? 20 : 8) - drainage * len, "soilWater");
  near(fluxes.eventOutputC * len, 51 * harvest, "eventOutputC amount");
  near(fluxes.eventOutputN * len, .85 * harvest, "eventOutputN amount");
  near(fluxes.nLeaching * len, leaching * len, "leaching amount");
  near(fluxes.nVolatilization * len, volatilization * len,
       "volatilization amount");
  near(fluxes.nUptake * len, uptake, "uptake amount");
  near(fluxes.nFixation * len, fixation, "fixation amount");
  near(fluxes.leafCreation * len, growth, "leaf growth amount");
  near(fluxes.woodCreation * len, growth, "wood growth amount");
  near(fluxes.fineRootCreation * len, growth, "fine root growth amount");
  near(fluxes.coarseRootCreation * len, growth, "coarse root growth amount");

  cleanupModel();
  deleteModelParams(testParams);
  gEvents = gEvent = NULL;

  checkFailures();
}

int main(void) {
  initFailureTracking(0);

  // Full Cartesian product, including independent empty storage/mineral pools.
  for (int harvest = 0; harvest < 2; harvest++)
    for (int leafOn = 0; leafOn < 3; leafOn++)
      for (int leafOff = 0; leafOff < 3; leafOff++)
        for (int fertilizer = 0; fertilizer < 2; fertilizer++)
          for (int sign = -1; sign <= 1; sign++)
            for (int storage = 0; storage < 2; storage++)
              for (int mineral = 0; mineral < 2; mineral++)
                for (int flooded = 0; flooded < 2; flooded++)
                  for (int losses = 0; losses < 2; losses++)
                    limitationCase(harvest, leafOn, leafOff, fertilizer, sign,
                                   storage, mineral, flooded, losses * .1);

  // Force mineral-loss scaling, with and without fertilizer and drainage.
  // Zero mean NPP isolates the loss guard from the growth limiter.
  for (int fertilizer = 0; fertilizer < 2; fertilizer++)
    for (int flooded = 0; flooded < 2; flooded++)
      limitationCase(0, 0, 0, fertilizer, 0, 0, 1, flooded, 1000);

  // Exhaust mineral N before limiting growth. These cases exercise both
  // guards together, including residual storage after two leaf-on events.
  for (int storage = 0; storage < 2; storage++)
    for (int flooded = 0; flooded < 2; flooded++)
      limitationCase(1, 2, 2, 0, 1, storage, 1, flooded, 1000);

  checkStatus("testLimitations");

  return 0;
}

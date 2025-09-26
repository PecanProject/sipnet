#include <stdio.h>
#include <stdlib.h>

#include "utils/tUtils.h"

#include "typesUtils.h"

int checkOutput(const char *stage, double expTillMod) {
  int status = 0;
  double curTillMod = eventTrackers.d_till_mod;
  if (!compareDoubles(expTillMod, curTillMod)) {
    logTest("%s: (day %d, hour %4.2f) tillage mod is %f, expected %f\n", stage,
            climate->day, climate->time, curTillMod, expTillMod);
    status = 1;
  }
  return status;
}

void decayMod(double *mod) { (*mod) *= exp(-climate->length / 30.0); }

void initEnv(void) { initEventTrackers(); }

int run(void) {
  int status = 0;
  double expTillMod;

  // We will need to switch back and forth between litter pool and soil manually
  prepTypesTest();

  // init values
  initEnv();

  //// ONE TILLAGE EVENT
  initEvents("events_one_tillage.in", 0);
  setupEvents();
  procEvents();

  // First tillage:
  expTillMod = 0.5;
  status |= checkOutput("pre-update", expTillMod);
  updateEventTrackers();
  decayMod(&expTillMod);
  status |= checkOutput("post-update", expTillMod);

  //// TWO TILLAGE EVENTS
  initEventTrackers();
  initEvents("events_two_tillage.in", 0);
  setupEvents();
  readClimData("events_two_tillage.clim");
  climate = firstClimate;

  expTillMod = 0.5;
  while (climate) {
    // This is kinda hacky, but fine for a test; add in effect of second tillage
    // when it is time
    if ((climate->day == 75) && (compareDoubles(climate->time, 0.0))) {
      expTillMod += 0.2;
    }

    procEvents();
    status |= checkOutput("pre-update", expTillMod);
    updateEventTrackers();
    decayMod(&expTillMod);
    status |= checkOutput("post-update", expTillMod);
    climate = climate->nextClim;
  }

  return status;
}

int main(void) {
  logTest("Starting run()\n");
  int status = run();
  if (status) {
    logTest("FAILED testEventTillage with status %d\n", status);
    exit(status);
  }

  logTest("PASSED testEventTillage\n");
}

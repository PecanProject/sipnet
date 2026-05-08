#include "limitations.h"

#include "common/context.h"

#include "nitrogen.h"
#include "state.h"

void checkNitrogenLimitation(void) {
  // Temp until refactor
  calcNFixationAndUptakeFluxes();
}

void checkLimitations() {
  // EXPECTED: calcLeafOnLimitation()

  if (ctx.nitrogenCycle) {
    checkNitrogenLimitation();
  }
}

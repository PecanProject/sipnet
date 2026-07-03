#include "depeffects.h"

#include <math.h>

#include "common/context.h"

#include "events.h"
#include "state.h"

double getClippedWaterFrac(double water, double whc) {
  return unitClip(water / whc);
}

double calcAnaerobicIndex(double water, double whc) {
  double f_whc = getClippedWaterFrac(water, whc);
  double f_a = params.fAnoxia;

  // Anaerobic index (oxygen limitation proxy)
  return unitClip((f_whc - f_a) / (1 - f_a));
}

double calcRespMoistEffect(double water, double whc) {
  double moistEffect;

  if ((!ctx.waterHResp) || (climate->tsoil < 0)) {
    // if not waterHResp, soil moisture does not affect heterotrophic
    // respiration; also, ignore moisture effects in frozen soils
    // :: from [2], snowpack addition
    moistEffect = 1.0;
  } else {
    double f_whc = getClippedWaterFrac(water, whc);
    if (!ctx.anaerobic) {
      // :: from [1], first part of eq (A20), with added exponent
      // Original formulation from [1], based on PnET is:
      //   moistEffect = water / whc
      // which matches here with params.soilRespMoistEffect=1 (the default
      // value)
      //
      // [TAG:UNKNOWN_PROVENANCE] soilRespMoistEffect
      // Note: older versions of sipnet note this as "using PnET formulation",
      // but we have been unable to verify that the exponent comes from PnET
      moistEffect = pow(f_whc, params.soilRespMoistEffect);
    } else {
      // Unimodal moisture response: suppressed under dry conditions, maximal
      // at intermediate moisture, reduced under saturated/anoxic conditions

      // Aerobic water availability (dry limitation)
      double D_aer = unitClip(f_whc / params.fAnoxia);
      // Anaerobic index (oxygen limitation proxy)
      double A = calcAnaerobicIndex(water, whc);
      // Uni-modal moisture response
      // Note that this will not go above 1, since:
      // - when f_whc <= f_a, A = 0, and moistEffect = D_aer
      // - when f_whc > f_a, D_aer caps at 1, and this becomes
      //   moistEffect = 1 - (1 - adr)A, and adr <= 1
      moistEffect = (1 - A) * D_aer + params.anaerobicDecompRate * A;
    }
  }

  return moistEffect;
}

double calcMethaneMoistEffect(double water, double whc) {
  // Anaerobic index (oxygen limitation proxy)
  double A = calcAnaerobicIndex(water, whc);

  return pow(A, params.anaerobicTransExp);
}

double calcTempEffect(double tsoil) {
  // :: from [1], D_temp calc as part of eq (A20)
  return pow(params.soilRespQ10, tsoil / 10);
}

double calcTillageEffect(void) { return 1 + eventTrackers.d_till_mod; }

double calcCNEffect(double kCN, double poolC, double poolN) {
  if (!ctx.nitrogenCycle) {
    return 1.0;
  }

  // CN ratio dependency term, using k_CN term that indicates CN value
  // at which the dependency is 1/2
  double cn = calcRatio(poolC, poolN);
  return kCN / (kCN + cn);
}

double calcVolatilizationMoistEffect(double water, double whc) {
  // Anaerobic index (oxygen limitation proxy)
  double A = calcAnaerobicIndex(water, whc);

  // 0.05 represents the baseline aerobic volatilization
  // The factor of 3.8 makes the max = 1, as with other dependency functions
  return 0.05 + 3.8 * A * (1 - A);
}

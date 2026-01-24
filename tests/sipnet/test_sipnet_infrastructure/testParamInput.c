#include <stdio.h>

#include "utils/tUtils.h"
#include "utils/exitHandler.c"
#include "common/logging.h"
#include "sipnet/sipnet.c"

void writeParams(const char *fname);

void resetParams(void) { memset(&params, 0, sizeof(struct Parameters)); }

void runNegTest(const char *fName) {
  // Pretty much guaranteed to leak memory, but that should be ok for a test
  ModelParams *modelParams = NULL;
  readParamData(&modelParams, fName);
}

int runTest(const char *root) {
  ModelParams *modelParams = NULL;

  char inName[50];
  char outName[50];
  char expName[50];
  int status;

  strcpy(inName, root);
  strcpy(outName, root);
  strcpy(expName, root);
  strcat(inName, ".param");
  strcat(outName, ".out");
  strcat(expName, ".exp");

  readParamData(&modelParams, inName);
  writeParams(outName);
  resetParams();
  deleteModelParams(modelParams);

  status = diffFiles(outName, expName);
  if (!status) {
    // Leave file for debugging if it failed
    remove(outName);
  }

  return status;
}

int run() {
  int status = 0;

  // exit() handling params
  int jmp_rval;

  // Step 0: all positive tests to run
  really_exit = 1;

  // First test - standard read
  status |= runTest("standard");

  // Second test - legacy read (param file with location column)
  status |= runTest("with_est");

  // Third test - error on '*' value
  really_exit = 0;
  should_exit = 1;
  exit_result = 1;  // reset for next test
  expected_code = EXIT_CODE_BAD_PARAMETER_VALUE;
  jmp_rval = setjmp(jump_env);
  if (!jmp_rval) {
    runNegTest("spatial_val.param");
  }
  test_assert(jmp_rval == 1);
  status |= !exit_result;
  if (!exit_result) {
    logTest("FAIL with spatial_val.param\n");
  }

  // Reset flags to allow main exit to work
  really_exit = 1;

  return status;
}

int main() {
  int status;

  status = run();
  if (status) {
    logTest("FAILED testParamInput with status %d\n", status);
    exit(status);
  }

  logTest("PASSED testParamInput\n");
}

void writeParams(const char *fname) {

  FILE *out = fopen(fname, "w");

  fprintf(out, "%s: %.2f\n", "plantWoodInit", params.plantWoodInit);
  fprintf(out, "%s: %.2f\n", "laiInit", params.laiInit);
  fprintf(out, "%s: %.2f\n", "litterInit", params.litterInit);
  fprintf(out, "%s: %.2f\n", "soilInit", params.soilInit);
  fprintf(out, "%s: %.2f\n", "soilWFracInit", params.soilWFracInit);
  fprintf(out, "%s: %.2f\n", "snowInit", params.snowInit);
  fprintf(out, "%s: %.2f\n", "aMax", params.aMax);
  fprintf(out, "%s: %.2f\n", "aMaxFrac", params.aMaxFrac);
  fprintf(out, "%s: %.2f\n", "baseFolRespFrac", params.baseFolRespFrac);
  fprintf(out, "%s: %.2f\n", "psnTMin", params.psnTMin);
  fprintf(out, "%s: %.2f\n", "psnTOpt", params.psnTOpt);
  fprintf(out, "%s: %.2f\n", "dVpdSlope", params.dVpdSlope);
  fprintf(out, "%s: %.2f\n", "dVpdExp", params.dVpdExp);
  fprintf(out, "%s: %.2f\n", "halfSatPar", params.halfSatPar);
  fprintf(out, "%s: %.2f\n", "attenuation", params.attenuation);
  fprintf(out, "%s: %.2f\n", "leafOnDay", params.leafOnDay);
  fprintf(out, "%s: %.2f\n", "gddLeafOn", params.gddLeafOn);
  fprintf(out, "%s: %.2f\n", "soilTempLeafOn", params.soilTempLeafOn);
  fprintf(out, "%s: %.2f\n", "leafOffDay", params.leafOffDay);
  fprintf(out, "%s: %.2f\n", "leafGrowth", params.leafGrowth);
  fprintf(out, "%s: %.2f\n", "fracLeafFall", params.fracLeafFall);
  fprintf(out, "%s: %.2f\n", "leafAllocation", params.leafAllocation);
  fprintf(out, "%s: %.2f\n", "leafTurnoverRate", params.leafTurnoverRate);
  fprintf(out, "%s: %.2f\n", "baseVegResp", params.baseVegResp);
  fprintf(out, "%s: %.2f\n", "vegRespQ10", params.vegRespQ10);
  fprintf(out, "%s: %.2f\n", "growthRespFrac", params.growthRespFrac);
  fprintf(out, "%s: %.2f\n", "frozenSoilFolREff", params.frozenSoilFolREff);
  fprintf(out, "%s: %.2f\n", "frozenSoilThreshold", params.frozenSoilThreshold);
  fprintf(out, "%s: %.2f\n", "litterBreakdownRate", params.litterBreakdownRate);
  fprintf(out, "%s: %.2f\n", "fracLitterRespired", params.fracLitterRespired);
  fprintf(out, "%s: %.2f\n", "baseSoilResp", params.baseSoilResp);
  fprintf(out, "%s: %.2f\n", "soilRespQ10", params.soilRespQ10);
  fprintf(out, "%s: %.2f\n", "soilRespMoistEffect", params.soilRespMoistEffect);
  fprintf(out, "%s: %.2f\n", "waterRemoveFrac", params.waterRemoveFrac);
  fprintf(out, "%s: %.2f\n", "frozenSoilEff", params.frozenSoilEff);
  fprintf(out, "%s: %.2f\n", "wueConst", params.wueConst);
  fprintf(out, "%s: %.2f\n", "soilWHC", params.soilWHC);
  fprintf(out, "%s: %.2f\n", "immedEvapFrac", params.immedEvapFrac);
  fprintf(out, "%s: %.2f\n", "fastFlowFrac", params.fastFlowFrac);
  fprintf(out, "%s: %.2f\n", "snowMelt", params.snowMelt);
  fprintf(out, "%s: %.2f\n", "rdConst", params.rdConst);
  fprintf(out, "%s: %.2f\n", "rSoilConst1", params.rSoilConst1);
  fprintf(out, "%s: %.2f\n", "rSoilConst2", params.rSoilConst2);
  fprintf(out, "%s: %.2f\n", "leafCSpWt", params.leafCSpWt);
  fprintf(out, "%s: %.2f\n", "cFracLeaf", params.cFracLeaf);
  fprintf(out, "%s: %.2f\n", "leafPoolDepth", params.leafPoolDepth);
  fprintf(out, "%s: %.2f\n", "woodTurnoverRate", params.woodTurnoverRate);
  fprintf(out, "%s: %.2f\n", "psnTMax", params.psnTMax);
  fprintf(out, "%s: %.2f\n", "fineRootFrac", params.fineRootFrac);
  fprintf(out, "%s: %.2f\n", "coarseRootFrac", params.coarseRootFrac);
  fprintf(out, "%s: %.2f\n", "fineRootAllocation", params.fineRootAllocation);
  fprintf(out, "%s: %.2f\n", "woodAllocation", params.woodAllocation);
  fprintf(out, "%s: %.2f\n", "fineRootExudation", params.fineRootExudation);
  fprintf(out, "%s: %.2f\n", "coarseRootExudation", params.coarseRootExudation);
  fprintf(out, "%s: %.2f\n", "coarseRootAllocation",
          params.coarseRootAllocation);
  fprintf(out, "%s: %.2f\n", "fineRootTurnoverRate",
          params.fineRootTurnoverRate);
  fprintf(out, "%s: %.2f\n", "coarseRootTurnoverRate",
          params.coarseRootTurnoverRate);
  fprintf(out, "%s: %.2f\n", "baseFineRootResp", params.baseFineRootResp);
  fprintf(out, "%s: %.2f\n", "baseCoarseRootResp", params.baseCoarseRootResp);
  fprintf(out, "%s: %.2f\n", "fineRootQ10", params.fineRootQ10);
  fprintf(out, "%s: %.2f\n", "coarseRootQ10", params.coarseRootQ10);

  fclose(out);
}

#include <stdio.h>
#include <stdlib.h>

#include "modelStructures.h"  // NOLINT
#include "utils/tUtils.h"
#include "sipnet/sipnet.c"

void writeParams(const char *fname);

int runTest(const char *root) {
  ModelParams *modelParams;
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

  status = diffFiles(outName, expName);
  if (!status) {
    // Leave file for debugging if it failed
    remove(outName);
  }

  return status;
}

int run() {
  int success = 0;

  // First test - standard read
  success |= runTest("standard");

  // Second test - legacy read (param file with location column)
  success |= runTest("with_est");

  return success;
}

int main() {
  int status;

  status = run();
  if (status) {
    printf("FAILED testParamInput with status %d\n", status);
    exit(status);
  }

  printf("PASSED testParamInput\n");
}

void writeParams(const char *fname) {

  FILE *out = fopen(fname, "w");

  fprintf(out, "%s: %.2f\n", "plantWoodInit", params.plantWoodInit);
  fprintf(out, "%s: %.2f\n", "laiInit", params.laiInit);
  fprintf(out, "%s: %.2f\n", "litterInit", params.litterInit);
  fprintf(out, "%s: %.2f\n", "soilInit", params.soilInit);
  fprintf(out, "%s: %.2f\n", "litterWFracInit", params.litterWFracInit);
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
  fprintf(out, "%s: %.2f\n", "baseSoilRespCold", params.baseSoilRespCold);
  fprintf(out, "%s: %.2f\n", "soilRespQ10", params.soilRespQ10);
  fprintf(out, "%s: %.2f\n", "soilRespQ10Cold", params.soilRespQ10Cold);
  fprintf(out, "%s: %.2f\n", "coldSoilThreshold", params.coldSoilThreshold);
  fprintf(out, "%s: %.2f\n", "E0", params.E0);
  fprintf(out, "%s: %.2f\n", "T0", params.T0);
  fprintf(out, "%s: %.2f\n", "soilRespMoistEffect", params.soilRespMoistEffect);
  fprintf(out, "%s: %.2f\n", "waterRemoveFrac", params.waterRemoveFrac);
  fprintf(out, "%s: %.2f\n", "frozenSoilEff", params.frozenSoilEff);
  fprintf(out, "%s: %.2f\n", "wueConst", params.wueConst);
  fprintf(out, "%s: %.2f\n", "litterWHC", params.litterWHC);
  fprintf(out, "%s: %.2f\n", "soilWHC", params.soilWHC);
  fprintf(out, "%s: %.2f\n", "immedEvapFrac", params.immedEvapFrac);
  fprintf(out, "%s: %.2f\n", "fastFlowFrac", params.fastFlowFrac);
  fprintf(out, "%s: %.2f\n", "snowMelt", params.snowMelt);
  fprintf(out, "%s: %.2f\n", "litWaterDrainRate", params.litWaterDrainRate);
  fprintf(out, "%s: %.2f\n", "rdConst", params.rdConst);
  fprintf(out, "%s: %.2f\n", "rSoilConst1", params.rSoilConst1);
  fprintf(out, "%s: %.2f\n", "rSoilConst2", params.rSoilConst2);
  fprintf(out, "%s: %.2f\n", "m_ballBerry", params.m_ballBerry);
  fprintf(out, "%s: %.2f\n", "leafCSpWt", params.leafCSpWt);
  fprintf(out, "%s: %.2f\n", "cFracLeaf", params.cFracLeaf);
  fprintf(out, "%s: %.2f\n", "leafPoolDepth", params.leafPoolDepth);
  fprintf(out, "%s: %.2f\n", "woodTurnoverRate", params.woodTurnoverRate);
  fprintf(out, "%s: %.2f\n", "psnTMax", params.psnTMax);
  fprintf(out, "%s: %.2f\n", "qualityLeaf", params.qualityLeaf);
  fprintf(out, "%s: %.2f\n", "qualityWood", params.qualityWood);
  fprintf(out, "%s: %.2f\n", "efficiency", params.efficiency);
  fprintf(out, "%s: %.2f\n", "maxIngestionRate", params.maxIngestionRate);
  fprintf(out, "%s: %.2f\n", "halfSatIngestion", params.halfSatIngestion);
  fprintf(out, "%s: %.2f\n", "totNitrogen", params.totNitrogen);
  fprintf(out, "%s: %.2f\n", "microbeNC", params.microbeNC);
  fprintf(out, "%s: %.2f\n", "microbeInit", params.microbeInit);
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
  fprintf(out, "%s: %.2f\n", "baseMicrobeResp", params.baseMicrobeResp);
  fprintf(out, "%s: %.2f\n", "microbeQ10", params.microbeQ10);
  fprintf(out, "%s: %.2f\n", "microbePulseEff", params.microbePulseEff);

  fclose(out);
}

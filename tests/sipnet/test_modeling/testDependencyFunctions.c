#include "utils/tUtils.h"
#include "sipnet/sipnet.c"

void setupTests() {
  // Set up the context
  initContext();
  ctx.litterPool = 1;
  ctx.nitrogenCycle = 1;
  ctx.waterHResp = 1;

  // Climate
  climate = (ClimateNode *)malloc(sizeof(ClimateNode));
  climate->year = 2024;
  climate->day = 70;
  climate->length = 0.125;
  climate->tsoil = 20.0;

  // Params; values picked for ease of calculation
  params.soilWHC = 10.0;
  params.soilRespMoistEffect = 2.0;
  params.kCN = 10.0;
  params.soilRespQ10 = 3.0;

  // Environment
  envi.soilWater = 5.0;
  // soil CN 7.5
  envi.soilC = 15.0;
  envi.soilOrgN = 2.0;
  // litter CN 5.0
  envi.litterC = 7.5;
  envi.litterN = 1.5;

  // Trackers
  initEventTrackers();
}

void initTestState(void) {
  ctx.waterHResp = 1;
  climate->tsoil = 20;
  eventTrackers.d_till_mod = 0.0;
}

void checkStatus(int status, const char *label, double calc, double exp) {
  if (status) {
    logTest("%s is %f, expected %f\n", label, calc, exp);
  }
}

int checkMoistEffect(double exp) {
  double calc = calcMoistEffect(envi.soilWater, params.soilWHC);
  int status = !compareDoubles(calc, exp);
  checkStatus(status, "Moist effect", calc, exp);
  return status;
}

int checkTempEffect(double exp) {
  double calc = calcTempEffect(climate->tsoil);
  int status = !compareDoubles(calc, exp);
  checkStatus(status, "Temp effect", calc, exp);
  return status;
}

int checkTillageEffect(double exp) {
  double calc = calcTillageEffect();
  int status = !compareDoubles(calc, exp);
  checkStatus(status, "Tillage effect", calc, exp);
  return status;
}

int checkCNRatio(double calc, double exp, const char *label) {
  int status = !compareDoubles(calc, exp);
  checkStatus(status, label, calc, exp);
  return status;
}

int checkCNEffect(double c, double n, double exp, const char *label) {
  double calc = calcCNEffect(params.kCN, c, n);
  int status = !compareDoubles(calc, exp);
  checkStatus(status, label, calc, exp);
  return status;
}

int runTests() {
  int status = 0;

  // Moisture effect, making note of the decision branches about ctx flags and
  // temp
  initTestState();
  status |= checkMoistEffect(0.25);
  ctx.waterHResp = 0;
  status |= checkMoistEffect(1);
  ctx.waterHResp = 1;
  climate->tsoil = -10.0;
  status |= checkMoistEffect(1);

  // Temp effect, no branches
  initTestState();
  status |= checkTempEffect(9.0);

  // Tillage effect, no branches
  // More complicated effects are tested elsewhere
  initTestState();
  status |= checkTillageEffect(1.0);
  eventTrackers.d_till_mod = 1.0;
  status |= checkTillageEffect(2.0);

  // C:N effect, one branch
  initTestState();
  status |= checkCNRatio(calcSoilCN(), 7.5, "Soil C:N ratio");
  status |= checkCNRatio(calcLitterCN(), 5.0, "Litter C:N ratio");
  status |=
      checkCNEffect(envi.soilC, envi.soilOrgN, 10.0 / 17.5, "Soil C:N effect");
  status |= checkCNEffect(envi.litterC, envi.litterN, 10.0 / 15.0,
                          "Litter C:N effect");
  // kCN should be CN value at which dep is 0.5, so let's check
  params.kCN = 5.0;
  status |= checkCNEffect(envi.litterC, envi.litterN, 0.5, "Litter C:N effect");

  // Lastly, dep is 1 when nitrogen_cycle is off
  ctx.nitrogenCycle = 0;
  status |= checkCNEffect(envi.soilC, envi.soilOrgN, 1.0, "Soil C:N effect");

  return status;
}

int run(void) {
  int status = 0;

  setupTests();

  status |= runTests();

  return status;
}

int main(void) {
  int status;

  logTest("Starting testDependencyFunctions:run()\n");
  status = run();
  if (status) {
    logTest("FAILED testDependencyFunctions with status %d\n", status);
    exit(status);
  }

  logTest("PASSED testDependencyFunctions\n");
}

#include "common/logging.h"
#include "utils/tUtils.h"

int testMissedEnviUpdate(void) {
  int status = 0;

  logTest("Starting testMissedEnviUpdate\n");

  int rc = runShell("gcc -Wall -I../../../src -c -o envi_fail.o envi_fail.c");

  status |= (rc == 0);
  if (status) {
    logTest("FAILED testMissedEnviUpdate\n");
  }

  return status;
}

int run(void) {
  int status = 0;

  status |= testMissedEnviUpdate();

  return status;
}

int main(void) {
  int status;

  logTest("Starting testRestartMissedEnvi\n");
  status = run();
  if (status) {
    logTest("FAILED testRestartMissedEnvi with status %d\n", status);
    exit(status);
  }

  logTest("PASSED testRestartMissedEnvi\n");
  return 0;
}

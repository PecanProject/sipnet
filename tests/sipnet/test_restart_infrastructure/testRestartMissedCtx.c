#include "common/logging.h"
#include "utils/tUtils.h"

int testMissedCtxUpdate(void) {
  int status = 0;

  logTest("Starting testMissedCtxUpdate\n");

  int rc = runShell("gcc -Wall -I../../../src -c -o ctx_fail.o ctx_fail.c");

  status |= (rc == 0);
  if (status) {
    logTest("FAILED testMissedCtxUpdate\n");
  }

  return status;
}

int run(void) {
  int status = 0;

  status |= testMissedCtxUpdate();

  return status;
}

int main(void) {
  int status;

  logTest("Starting testRestartMissedCtx\n");
  status = run();
  if (status) {
    logTest("FAILED testRestartMissedCtx with status %d\n", status);
    exit(status);
  }

  logTest("PASSED testRestartMissedCtx\n");
  return 0;
}

#include "common/logging.h"
#include "utils/tUtils.h"

#define FAIL_MSG_1 "static assertion failed"
#define FAIL_MSG_2 "Model flags changed"

int testMissedCtxUpdate(void) {
  int status = 0;

  logTest("Starting testMissedCtxUpdate\n");

  int rc = runShell("gcc -Wall -I../../../src -c -o ctx_fail.o "
                    "bad_code/ctx_fail.c > ctx.log 2>&1");

  status |= (rc == 0);
  status |= !fileContains("ctx.log", FAIL_MSG_1);
  status |= !fileContains("ctx.log", FAIL_MSG_2);

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

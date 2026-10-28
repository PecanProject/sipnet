#include "utils/tUtils.h"
#include "utils/exitHandler.c"
#include "sipnet/sipnet.c"

typedef struct {
  const char *name;
  double leaf;
  double wood;
  double fineRoot;
  int rejects;
  double coarseRoot;
} AllocationCase;

static int runCase(const AllocationCase *test) {
  memset(&params, 0, sizeof(params));
  params.leafAllocation = test->leaf;
  params.woodAllocation = test->wood;
  params.fineRootAllocation = test->fineRoot;

  really_exit = 0;
  should_exit = test->rejects;
  expected_code = EXIT_CODE_BAD_PARAMETER_VALUE;
  exit_result = 1;

  int jumped = setjmp(jump_env);
  if (!jumped) {
    ensureAllocation();
  }
  test_assert(jumped == test->rejects);
  if (!test->rejects) {
    test_assert(params.coarseRootAllocation > 0.0);
    test_assert(compareDoubles(params.coarseRootAllocation, test->coarseRoot));
  }
  test_assert(params.leafAllocation == test->leaf);
  test_assert(params.woodAllocation == test->wood);
  test_assert(params.fineRootAllocation == test->fineRoot);

  really_exit = 1;
  if (!exit_result) {
    logTest("FAIL allocation case: %s\n", test->name);
  }
  return !exit_result;
}

int main(void) {
  const AllocationCase cases[] = {
      {"negative leaf", -0.125, 0.25, 0.25, 1, 0},
      {"negative wood", 0.25, -0.125, 0.25, 1, 0},
      {"negative fine root", 0.25, 0.25, -0.125, 1, 0},
      {"small negative leaf", -1e-12, 0.25, 0.25, 1, 0},
      {"all zero", 0, 0, 0, 1, 0},
      {"zero leaf", 0, 0.5, 0.25, 1, 0},
      {"zero wood", 0.25, 0, 0.25, 1, 0},
      {"zero fine root", 0.25, 0.25, 0, 1, 0},
      {"negative zero leaf", -0.0, 0.25, 0.25, 1, 0},
      {"negative zero wood", 0.25, -0.0, 0.25, 1, 0},
      {"negative zero fine root", 0.25, 0.25, -0.0, 1, 0},
      {"small positive leaf", 1e-12, 0.25, 0.25, 0, 0.5 - 1e-12},
      {"small positive wood", 0.25, 1e-12, 0.25, 0, 0.5 - 1e-12},
      {"small positive fine root", 0.25, 0.25, 1e-12, 0, 0.5 - 1e-12},
      {"small positive coarse root", 0.25, 0.25, 0.5 - 1e-12, 0, 1e-12},
      {"sum below one", 0.25, 0.25, 0.25, 0, 0.25},
      {"sum exactly one", 0.25, 0.25, 0.5, 1, 0},
      {"leaf equals one", 1, 0, 0, 1, 0},
      {"wood equals one", 0, 1, 0, 1, 0},
      {"fine root equals one", 0, 0, 1, 1, 0},
      {"leaf above one", 1.125, 0, 0, 1, 0},
      {"wood above one", 0, 1.125, 0, 1, 0},
      {"fine root above one", 0, 0, 1.125, 1, 0},
      {"sum above one", 0.5, 0.375, 0.25, 1, 0},
  };

  int status = 0;
  for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
    status |= runCase(&cases[i]);
  }
  if (status) {
    logTest("FAILED testAllocation with status %d\n", status);
  } else {
    logTest("PASSED testAllocation (%zu cases)\n",
            sizeof(cases) / sizeof(cases[0]));
  }
  return status;
}

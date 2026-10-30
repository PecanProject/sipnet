#include <float.h>

#include "utils/tUtils.h"
#include "utils/exitHandler.c"
#include "sipnet/sipnet.c"

typedef struct {
  const char *name;
  const char *input;
  int required;
  int rejects;
  int numRead;
  double expected;
} ParameterCase;

static double parsedValue;

static int runParameterCase(const ParameterCase *test) {
  parsedValue = 123.0;
  ModelParams *model = newModelParams(1);
  initializeOneModelParam(model, "quantity", &parsedValue, test->required);
  FILE *input = tmpfile();
  if (input == NULL) {
    logTest("FAIL %s: unable to create temporary input\n", test->name);
    deleteModelParams(model);
    return 1;
  }
  fputs(test->input, input);
  rewind(input);

  really_exit = 0;
  should_exit = test->rejects;
  expected_code = EXIT_CODE_BAD_PARAMETER_VALUE;
  exit_result = 1;
  int jumped = setjmp(jump_env);
  if (!jumped) {
    readModelParams(model, input);
  }
  test_assert(jumped == test->rejects);
  test_assert(model->numParamsRead == test->numRead);
  test_assert(model->params[0].isRead == test->numRead);
  test_assert(parsedValue == test->expected);
  really_exit = 1;
  fclose(input);
  deleteModelParams(model);
  if (!exit_result) {
    logTest("FAIL parameter case: %s\n", test->name);
  }
  return !exit_result;
}

static int runAllocationCase(int field, double value) {
  memset(&params, 0, sizeof(params));
  params.leafAllocation = 0.25;
  params.woodAllocation = 0.25;
  params.fineRootAllocation = 0.25;
  params.coarseRootAllocation = 123.0;
  switch (field) {
    case 0:
      params.leafAllocation = value;
      break;
    case 1:
      params.woodAllocation = value;
      break;
    case 2:
      params.fineRootAllocation = value;
      break;
  }

  really_exit = 0;
  should_exit = 1;
  expected_code = EXIT_CODE_BAD_PARAMETER_VALUE;
  exit_result = 1;
  int jumped = setjmp(jump_env);
  if (!jumped) {
    ensureAllocation();
  }
  test_assert(jumped == 1);
  test_assert(params.coarseRootAllocation == 123.0);
  really_exit = 1;
  if (!exit_result) {
    logTest("FAIL allocation field %d with nonfinite value %.17g\n", field,
            value);
  }
  return !exit_result;
}

int main(void) {
  const ParameterCase cases[] = {
      {"NaN", "quantity nan\n", 1, 1, 0, 123},
      {"uppercase NaN", "quantity NAN\n", 1, 1, 0, 123},
      {"NaN payload", "quantity nan(payload)\n", 1, 1, 0, 123},
      {"positive infinity", "quantity inf\n", 1, 1, 0, 123},
      {"signed infinity", "quantity +inf\n", 1, 1, 0, 123},
      {"negative infinity", "quantity -inf\n", 1, 1, 0, 123},
      {"Infinity spelling", "quantity Infinity\n", 1, 1, 0, 123},
      {"positive overflow", "quantity 1e999\n", 1, 1, 0, 123},
      {"negative overflow", "quantity -1e999\n", 1, 1, 0, 123},
      {"case-insensitive name", "QUANTITY nan\n", 1, 1, 0, 123},
      {"supplied optional NaN", "quantity nan\n", 0, 1, 0, 123},
      {"finite zero", "quantity 0\n", 1, 0, 1, 0},
      {"finite fraction", "quantity 0.25\n", 1, 0, 1, 0.25},
      {"finite signed value", "quantity -12.5\n", 1, 0, 1, -12.5},
      {"finite maximum", "quantity 1.7976931348623157e308\n", 1, 0, 1, DBL_MAX},
      {"omitted optional", "", 0, 0, 0, 123},
      {"ignored unknown NaN", "legacy nan\nquantity 0.25\n", 1, 0, 1, 0.25},
      {"legacy extra columns", "quantity 0.25 1 0 1 0.1\n", 1, 0, 1, 0.25},
  };
  const double nonfinite[] = {NAN, INFINITY, -INFINITY};
  int status = 0;
  for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
    status |= runParameterCase(&cases[i]);
  }
  for (int field = 0; field < 3; ++field) {
    for (size_t i = 0; i < sizeof(nonfinite) / sizeof(nonfinite[0]); ++i) {
      status |= runAllocationCase(field, nonfinite[i]);
    }
  }
  logTest("%s testNonfiniteParams (18 reader cases, 9 allocation cases)\n",
          status ? "FAILED" : "PASSED");
  return status;
}

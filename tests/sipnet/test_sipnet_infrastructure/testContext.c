#include "common/context.h"
#include "utils/tUtils.h"
#include "utils/exitHandler.c"

static int checkLookup(const char *name, struct context_metadata *expected) {
  if (getContextMetadata(name) != expected) {
    logTest("Unexpected metadata for context key %s\n", name);
    return 1;
  }
  return 0;
}

static int checkOverlongKey(const char *name) {
  really_exit = 0;
  should_exit = 1;
  expected_code = EXIT_CODE_BAD_PARAMETER_VALUE;
  exit_result = 1;

  if (setjmp(jump_env) == 0) {
    getContextMetadata(name);
    test_assert(0);
  }
  really_exit = 1;

  if (!exit_result) {
    logTest("Expected configuration error for a %zu-character key\n",
            strlen(name));
  }
  return !exit_result;
}

int main(void) {
  int status = 0;
  int testValue = 0;
  char name[701];

  really_exit = 1;
  initContext();

  // The longest supported normalized key must fit with its null terminator.
  memset(name, 'x', CONTEXT_CHAR_MAXLEN - 1);
  name[CONTEXT_CHAR_MAXLEN - 1] = '\0';
  createContextMetadata(name, "boundary", CTX_DEFAULT, CTX_INT, &testValue, 0);
  struct context_metadata *boundary = getContextMetadata(name);
  if (boundary == NULL || strcmp(boundary->keyName, name) != 0) {
    logTest("Failed to register a 255-character context key\n");
    status = 1;
  }

  // An oversized spelling must not truncate to the registered boundary key.
  name[CONTEXT_CHAR_MAXLEN - 1] = 'y';
  name[CONTEXT_CHAR_MAXLEN] = '\0';
  status |= checkOverlongKey(name);
  name[CONTEXT_CHAR_MAXLEN] = 'z';
  name[CONTEXT_CHAR_MAXLEN + 1] = '\0';
  status |= checkOverlongKey(name);
  memset(name, 'x', sizeof(name) - 1);
  name[sizeof(name) - 1] = '\0';
  status |= checkOverlongKey(name);

  // Short unknown keys, normalization, and the legacy alias still work.
  status |= checkLookup("unknown-key", NULL);
  struct context_metadata *snow = getContextMetadata("snow");
  struct context_metadata *prefix = getContextMetadata("filePrefix");
  if (snow == NULL || prefix == NULL) {
    logTest("Missing standard context metadata\n");
    status = 1;
  }
  status |= checkLookup("S_n-O_w", snow);
  status |= checkLookup("FILE_NAME", prefix);
  status |= checkLookup("fIlE---nAmE", prefix);

  // Count retained characters, not the length of the original spelling.
  memset(name, '_', sizeof(name) - 5);
  strcpy(name + sizeof(name) - 5, "SnOw");
  status |= checkLookup(name, snow);

  freeContextMetadata();
  if (status) {
    logTest("FAILED testContext with status %d\n", status);
    return status;
  }
  logTest("PASSED testContext\n");
  return 0;
}

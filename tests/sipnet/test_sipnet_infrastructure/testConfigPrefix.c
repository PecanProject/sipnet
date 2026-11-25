#include <stdio.h>
#include <string.h>

#include "common/context.h"
#include "utils/tUtils.h"

#define CONFIG_FILE "prefix_validation.tmp"
#define LOG_FILE "prefix_validation.log"
#define SHORT_PREFIX "prefix_validation_missing"
#define BASE_ARGS "--no-events --no-do-main-output --no-dump-config"

static int checkPrefix(const char *key, const char *value,
                       const char *cliPrefix, int expectedCode,
                       const char *expectedText) {
  FILE *config = fopen(CONFIG_FILE, "w");
  if (config == NULL) {
    logTest("Unable to create %s\n", CONFIG_FILE);
    return 1;
  }
  fprintf(config, "%s %s\n", key, value);
  if (fclose(config) != 0) {
    logTest("Unable to close %s\n", CONFIG_FILE);
    remove(CONFIG_FILE);
    return 1;
  }

  char args[CONTEXT_CHAR_MAXLEN + 128];
  if (cliPrefix == NULL) {
    snprintf(args, sizeof(args), "%s", BASE_ARGS);
  } else {
    snprintf(args, sizeof(args), "%s -f '%s'", BASE_ARGS, cliPrefix);
  }
  int code = runModelWithArgs(CONFIG_FILE, LOG_FILE, args);
  int status = code != expectedCode || !fileContains(LOG_FILE, expectedText);
  if (status) {
    logTest("FAILED %s (value length %zu, CLI %s): exit %d, expected %d "
            "and diagnostic '%s'\n",
            key, strlen(value), cliPrefix == NULL ? "unset" : "set", code,
            expectedCode, expectedText);
  }
  remove(CONFIG_FILE);
  remove(LOG_FILE);
  return status;
}

// Accepted prefixes deliberately reach a missing .param file. This checks the
// effective filename without needing model inputs or producing model outputs.
static int checkAccepted(const char *key, const char *value,
                         const char *cliPrefix) {
  const char *effective = cliPrefix == NULL ? value : cliPrefix;
  char expected[CONTEXT_CHAR_MAXLEN + 32];
  snprintf(expected, sizeof(expected), "Error reading '%s.param'", effective);
  return checkPrefix(key, value, cliPrefix, EXIT_CODE_FILE_OPEN_OR_READ_ERROR,
                     expected);
}

int main(void) {
  int status = 0;
  const char *keys[] = {"FILE_PREFIX", "FILE_NAME"};
  const size_t lengths[] = {FILENAME_PREFIX_MAXLEN, FILENAME_PREFIX_MAXLEN + 1,
                            253, CONTEXT_CHAR_MAXLEN - 1, CONTEXT_CHAR_MAXLEN};
  char prefix[CONTEXT_CHAR_MAXLEN + 1];

  logTest("Starting testConfigPrefix\n");
  for (size_t key = 0; key < sizeof(keys) / sizeof(keys[0]); ++key) {
    status |= checkAccepted(keys[key], SHORT_PREFIX, NULL);
    for (size_t ind = 0; ind < sizeof(lengths) / sizeof(lengths[0]); ++ind) {
      size_t length = lengths[ind];
      memset(prefix, 'p', length);
      prefix[length] = '\0';
      if (length <= FILENAME_PREFIX_MAXLEN) {
        status |= checkAccepted(keys[key], prefix, NULL);
      } else {
        const char *expected = length >= CONTEXT_CHAR_MAXLEN
                                   ? "exceeds maximum length"
                                   : "too long; max length is";
        status |= checkPrefix(keys[key], prefix, NULL,
                              EXIT_CODE_BAD_PARAMETER_VALUE, expected);
      }
    }
    status |=
        checkPrefix(keys[key], "none", NULL, EXIT_CODE_BAD_PARAMETER_VALUE,
                    "filePrefix must be set");

    // Only the final effective prefix is validated: valid CLI values win over
    // both oversized and empty file values.
    memset(prefix, 'p', 253);
    prefix[253] = '\0';
    status |= checkAccepted(keys[key], prefix, SHORT_PREFIX);
    status |= checkAccepted(keys[key], "none", SHORT_PREFIX);
  }

  status |=
      checkPrefix("file-prefix", prefix, NULL, EXIT_CODE_BAD_PARAMETER_VALUE,
                  "too long; max length is");
  status |= checkPrefix("FILE_PREFIX", SHORT_PREFIX, prefix,
                        EXIT_CODE_BAD_CLI_ARGUMENT, "exceeds maximum length");
  status |=
      checkPrefix("FILE_PREFIX", SHORT_PREFIX, "",
                  EXIT_CODE_BAD_PARAMETER_VALUE, "filePrefix must be set");

  if (status) {
    logTest("FAILED testConfigPrefix\n");
    return 1;
  }
  logTest("PASSED testConfigPrefix\n");
  return 0;
}

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/logging.h"
#include "sipnet/state.h"
#include "utils/tUtils.h"

#define TEST_WORK_DIR "../../../tests/smoke/russell_1"
#define DEBUG_PREFIX "debug_logs/sipnet_debug"
#define ENVI_FILE TEST_WORK_DIR "/" DEBUG_PREFIX "_envi.log"
#define FLUXES_FILE TEST_WORK_DIR "/" DEBUG_PREFIX "_fluxes.log"
#define TRACKERS_FILE TEST_WORK_DIR "/" DEBUG_PREFIX "_trackers.log"
#define SIPNET_OUT_FILE TEST_WORK_DIR "/sipnet.out"

#define EXPECTED_ENVI_FIELDS ((int)(sizeof(Envi) / sizeof(double)))
#define EXPECTED_FLUX_FIELDS ((int)(sizeof(Fluxes) / sizeof(double)))
// These are not all doubles
// Let's do a little more work for Trackers
#define NUM_TRACKER_INTS 1
#define TRACKERS_SIZE (int)(sizeof(Trackers) - NUM_TRACKER_INTS * sizeof(int))
#define EXPECTED_TRACKER_FIELDS                                                \
  ((int)(TRACKERS_SIZE / sizeof(double)) + NUM_TRACKER_INTS)
#define EXPECTED_PHENOLOGY_FIELDS 3
#define EXPECTED_SURVIVAL_FIELDS 1

static int countTokens(const char *line) {
  int count = 0;
  int inToken = 0;

  for (int ind = 0; line[ind] != '\0' && line[ind] != '\n'; ++ind) {
    if (line[ind] == ' ' || line[ind] == '\t') {
      inToken = 0;
    } else if (!inToken) {
      inToken = 1;
      ++count;
    }
  }

  return count;
}

static int countLines(const char *path) {
  FILE *in = fopen(path, "r");
  int count = 0;
  int ch;

  if (in == NULL) {
    logTest("Unable to open %s\n", path);
    return -1;
  }

  while ((ch = fgetc(in)) != EOF) {
    if (ch == '\n') {
      ++count;
    }
  }

  fclose(in);
  return count;
}

static int checkHeader(const char *path, int expectedTokens,
                       const char *requiredToken1, const char *requiredToken2) {
  FILE *in = fopen(path, "r");
  char line[8192];

  if (in == NULL) {
    logTest("Unable to open %s\n", path);
    return 1;
  }
  if (fgets(line, sizeof(line), in) == NULL) {
    fclose(in);
    logTest("Unable to read header from %s\n", path);
    return 1;
  }
  fclose(in);

  if (strncmp(line, "year day time ", strlen("year day time ")) != 0) {
    logTest("Header in %s does not start with year/day/time\n", path);
    return 1;
  }
  if (countTokens(line) != expectedTokens) {
    logTest("Unexpected token count in %s header: got %d expected %d\n", path,
            countTokens(line), expectedTokens);
    return 1;
  }
  if (!strstr(line, requiredToken1) || !strstr(line, requiredToken2)) {
    logTest("Header in %s missing required debug tokens %s and/or %s\n", path,
            requiredToken1, requiredToken2);
    return 1;
  }

  return 0;
}

int run(void) {
  int status = 0;
  char cmd[1024];

  snprintf(cmd, sizeof(cmd),
           "cd %s && ../../../sipnet -i sipnet.in --debug-log %s > %s 2>&1",
           TEST_WORK_DIR, DEBUG_PREFIX, "debug_log_test.log");
  status = runShell(cmd);
  if (status != 0) {
    logTest("sipnet failed with status %d\n", status);
    return status;
  }

  status |= checkHeader(ENVI_FILE, 3 + EXPECTED_ENVI_FIELDS, "plantWoodC",
                        "plantCAccountingDelta");
  status |= checkHeader(FLUXES_FILE, 3 + EXPECTED_FLUX_FIELDS, "photosynthesis",
                        "litterMethane");
  status |=
      checkHeader(TRACKERS_FILE,
                  3 + EXPECTED_TRACKER_FIELDS + EXPECTED_PHENOLOGY_FIELDS +
                      EXPECTED_SURVIVAL_FIELDS,
                  "t.gpp", "pt.lastYear");

  int mainLines = countLines(SIPNET_OUT_FILE);
  int enviLines = countLines(ENVI_FILE);
  int fluxLines = countLines(FLUXES_FILE);
  int trackerLines = countLines(TRACKERS_FILE);

  if (mainLines < 0 || enviLines < 0 || fluxLines < 0 || trackerLines < 0) {
    return 1;
  }
  if (mainLines != enviLines || mainLines != fluxLines ||
      mainLines != trackerLines) {
    logTest("Debug log line counts do not match sipnet.out (%d, %d, %d, %d)\n",
            mainLines, enviLines, fluxLines, trackerLines);
    status = 1;
  }

  return status;
}

int init(void) {
  int status = 0;

  status |= runShell("cd " TEST_WORK_DIR
                     " && rm -rf debug_logs && mkdir -p debug_logs");

  status |=
      runShell("cd " TEST_WORK_DIR " && cp sipnet.config sipnet.config.orig");

  if (status != 0) {
    logTest("Could not initialize test directory %s, failed with status %d\n",
            TEST_WORK_DIR, status);
    return status;
  }

  return status;
}

int cleanup(void) {
  int status = 0;

  status |=
      runShell("cd " TEST_WORK_DIR " && cp sipnet.config.orig sipnet.config && "
               " rm -f sipnet.config.orig");

  status |= runShell("cd " TEST_WORK_DIR
                     " && rm -rf debug_logs && rm -f debug_log_test.log");
  if (status != 0) {
    logTest("Could not clean up test directory %s, failed with status %d.\n"
            "There are likely modified source file(s) and extra log files "
            "still in place\n",
            TEST_WORK_DIR, status);
    return status;
  }

  return status;
}

int main(void) {
  int status = 0;

  logTest("Starting testDebugLogFiles\n");

  status |= init();

  // If init() fails, don't run(); but, we'll want to attempt cleanup()
  if (!status) {
    status |= run();
  }

  status |= cleanup();

  if (status) {
    logTest("FAILED testDebugLogFiles with status %d\n", status);
    exit(status);
  }

  logTest("PASSED testDebugLogFiles\n");
  return 0;
}

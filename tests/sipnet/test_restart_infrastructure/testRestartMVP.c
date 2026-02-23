#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#include "common/logging.h"
#include "utils/tUtils.h"

#define SIPNET_CMD "../../../sipnet"
#define CHECKPOINT_FILE "run.restart"
#define RESTART_MAGIC_LINE "SIPNET_RESTART_ASCII 1.0"

static int runShell(const char *cmd) {
  int rc = system(cmd);
  if (rc == -1) {
    logTest("system() failed for command: %s\n", cmd);
    return 255;
  }

  if (WIFEXITED(rc)) {
    return WEXITSTATUS(rc);
  }

  return 255;
}

static int prepRunFiles(const char *climFile) {
  int status = 0;
  status |= copyFile((char *)"restart.param", (char *)"run.param");
  status |= copyFile((char *)climFile, (char *)"run.clim");
  status |= copyFile((char *)"events_base.in", (char *)"events.in");
  return status;
}

static int runModel(const char *inputFile, const char *logFile) {
  char cmd[512];
  sprintf(cmd, "%s -i %s > %s 2>&1", SIPNET_CMD, inputFile, logFile);
  return runShell(cmd);
}

static int truncateFileToSize(const char *file, long size) {
  if (truncate(file, size) != 0) {
    logTest("Unable to truncate %s to %ld bytes\n", file, size);
    return 1;
  }
  return 0;
}

static int fileStartsWith(const char *file, const char *expectedPrefix) {
  FILE *fp = fopen(file, "r");
  if (fp == NULL) {
    logTest("Unable to open %s\n", file);
    return 0;
  }

  char line[512];
  if (fgets(line, sizeof(line), fp) == NULL) {
    fclose(fp);
    return 0;
  }
  fclose(fp);

  line[strcspn(line, "\r\n")] = '\0';
  return strcmp(line, expectedPrefix) == 0;
}

static int replaceFirstOccurrence(const char *file, const char *needle,
                                  const char *replacement) {
  FILE *in = fopen(file, "r");
  if (in == NULL) {
    logTest("Unable to open %s for reading\n", file);
    return 1;
  }

  if (fseek(in, 0, SEEK_END) != 0) {
    fclose(in);
    return 1;
  }
  long size = ftell(in);
  if (size < 0) {
    fclose(in);
    return 1;
  }
  if (fseek(in, 0, SEEK_SET) != 0) {
    fclose(in);
    return 1;
  }

  char *buffer = (char *)malloc((size_t)size + 1);
  if (buffer == NULL) {
    fclose(in);
    return 1;
  }
  if (fread(buffer, 1, (size_t)size, in) != (size_t)size) {
    free(buffer);
    fclose(in);
    return 1;
  }
  buffer[size] = '\0';
  fclose(in);

  char *pos = strstr(buffer, needle);
  if (pos == NULL) {
    free(buffer);
    logTest("Could not find '%s' in %s\n", needle, file);
    return 1;
  }

  size_t beforeLen = (size_t)(pos - buffer);
  size_t needleLen = strlen(needle);
  size_t replacementLen = strlen(replacement);
  size_t afterLen = strlen(pos + needleLen);
  size_t newLen = beforeLen + replacementLen + afterLen;

  char *newContent = (char *)malloc(newLen + 1);
  if (newContent == NULL) {
    free(buffer);
    return 1;
  }

  memcpy(newContent, buffer, beforeLen);
  memcpy(newContent + beforeLen, replacement, replacementLen);
  memcpy(newContent + beforeLen + replacementLen, pos + needleLen, afterLen);
  newContent[newLen] = '\0';

  FILE *out = fopen(file, "w");
  if (out == NULL) {
    free(buffer);
    free(newContent);
    return 1;
  }
  if (fwrite(newContent, 1, newLen, out) != newLen) {
    free(buffer);
    free(newContent);
    fclose(out);
    return 1;
  }

  free(buffer);
  free(newContent);
  fclose(out);
  return 0;
}

static int hasManagedEventOnDay(const char *eventFile, int year, int day) {
  FILE *in = fopen(eventFile, "r");
  if (in == NULL) {
    logTest("Unable to open %s\n", eventFile);
    return 1;
  }

  char line[1024];
  int found = 0;
  while (fgets(line, sizeof(line), in) != NULL) {
    int evYear, evDay;
    char evType[32];
    int n = sscanf(line, "%d %d %31s", &evYear, &evDay, evType);
    if (n == 3 && evYear == year && evDay == day) {
      if (strcmp(evType, "fert") == 0 || strcmp(evType, "till") == 0 ||
          strcmp(evType, "irrig") == 0 || strcmp(evType, "plant") == 0 ||
          strcmp(evType, "harv") == 0) {
        found = 1;
        break;
      }
    }
  }

  fclose(in);
  return found;
}

static int testSegmentedEquivalence(void) {
  int status = 0;
  int stepStatus = 0;

  runShell("rm -f run.out events.out run.restart continuous.out seg1.out "
           "seg2.out segmented_joined.out *.log");

  stepStatus = prepRunFiles("restart_full.clim");
  if (stepStatus) {
    logTest("Failed to prepare files for continuous run\n");
    return stepStatus;
  }

  status |= (runModel("restart_cont.in", "continuous.log") != 0);
  status |= rename("run.out", "continuous.out");
  status |= rename("events.out", "continuous.events");

  stepStatus = prepRunFiles("restart_segment1.clim");
  if (stepStatus) {
    logTest("Failed to prepare files for segment 1\n");
    return status | stepStatus;
  }

  status |= (runModel("restart_seg1.in", "seg1.log") != 0);
  status |= !fileStartsWith(CHECKPOINT_FILE, RESTART_MAGIC_LINE);
  status |= rename("run.out", "seg1.out");
  status |= rename("events.out", "seg1.events");

  stepStatus = prepRunFiles("restart_segment2.clim");
  if (stepStatus) {
    logTest("Failed to prepare files for segment 2\n");
    return status | stepStatus;
  }

  status |= (runModel("restart_seg2.in", "seg2.log") != 0);
  status |= rename("run.out", "seg2.out");
  status |= rename("events.out", "seg2.events");

  // Stitch outputs in test to compare model state continuity.
  status |= runShell("cat seg1.out > segmented_joined.out");
  status |= runShell("tail -n +2 seg2.out >> segmented_joined.out");

  status |= diffFiles("continuous.out", "segmented_joined.out");

  // Multi-event boundary day must not be replayed in resumed segment.
  status |= hasManagedEventOnDay("seg2.events", 2016, 47);

  if (status) {
    logTest("testSegmentedEquivalence failed\n");
  }

  return status;
}

static int testStrictClimateMismatchFails(void) {
  int status = 0;
  int stepStatus = 0;
  int rc;

  runShell("rm -f run.out events.out run.restart *.log");

  stepStatus = prepRunFiles("restart_segment1.clim");
  if (stepStatus) {
    logTest("Failed to prepare files for mismatch test segment 1\n");
    return stepStatus;
  }

  status |= (runModel("restart_seg1.in", "mismatch_seg1.log") != 0);

  stepStatus = prepRunFiles("restart_segment2_bad.clim");
  if (stepStatus) {
    logTest("Failed to prepare files for mismatch test segment 2\n");
    return status | stepStatus;
  }

  rc = runModel("restart_seg2_bad.in", "mismatch_seg2.log");
  status |= (rc == 0);

  if (status) {
    logTest("testStrictClimateMismatchFails failed (rc=%d)\n", rc);
  }

  return status;
}

static int testNoRestartModeUnchanged(void) {
  int status = 0;
  int stepStatus = 0;

  runShell("rm -f run.out events.out *.log no_restart_a.out no_restart_b.out");

  stepStatus = prepRunFiles("restart_full.clim");
  if (stepStatus) {
    logTest("Failed to prepare files for no-restart A\n");
    return stepStatus;
  }
  status |= (runModel("norestart_a.in", "norestart_a.log") != 0);
  status |= rename("run.out", "no_restart_a.out");

  stepStatus = prepRunFiles("restart_full.clim");
  if (stepStatus) {
    logTest("Failed to prepare files for no-restart B\n");
    return status | stepStatus;
  }
  status |= (runModel("norestart_b.in", "norestart_b.log") != 0);
  status |= rename("run.out", "no_restart_b.out");

  status |= diffFiles("no_restart_a.out", "no_restart_b.out");

  if (status) {
    logTest("testNoRestartModeUnchanged failed\n");
  }

  return status;
}

static int testModelVersionMismatchFails(void) {
  int status = 0;
  int stepStatus = 0;
  int rc;

  runShell("rm -f run.out events.out run.restart *.log");

  stepStatus = prepRunFiles("restart_segment1.clim");
  if (stepStatus) {
    logTest(
        "Failed to prepare files for model-version mismatch test segment 1\n");
    return stepStatus;
  }
  status |= (runModel("restart_seg1.in", "model_mismatch_seg1.log") != 0);
  status |= replaceFirstOccurrence(CHECKPOINT_FILE, "model_version ",
                                   "model_version X");

  stepStatus = prepRunFiles("restart_segment2.clim");
  if (stepStatus) {
    logTest(
        "Failed to prepare files for model-version mismatch test segment 2\n");
    return status | stepStatus;
  }

  rc = runModel("restart_seg2.in", "model_mismatch_seg2.log");
  status |= (rc == 0);

  if (status) {
    logTest("testModelVersionMismatchFails failed (rc=%d)\n", rc);
  }

  return status;
}

static int testSchemaMismatchFails(void) {
  int status = 0;
  int stepStatus = 0;
  int rc;

  runShell("rm -f run.out events.out run.restart *.log");

  stepStatus = prepRunFiles("restart_segment1.clim");
  if (stepStatus) {
    logTest("Failed to prepare files for schema mismatch test segment 1\n");
    return stepStatus;
  }
  status |= (runModel("restart_seg1.in", "schema_mismatch_seg1.log") != 0);
  status |= replaceFirstOccurrence(CHECKPOINT_FILE, "SIPNET_RESTART_ASCII 1.0",
                                   "SIPNET_RESTART_ASCII 9.9");

  stepStatus = prepRunFiles("restart_segment2.clim");
  if (stepStatus) {
    logTest("Failed to prepare files for schema mismatch test segment 2\n");
    return status | stepStatus;
  }

  rc = runModel("restart_seg2.in", "schema_mismatch_seg2.log");
  status |= (rc == 0);

  if (status) {
    logTest("testSchemaMismatchFails failed (rc=%d)\n", rc);
  }

  return status;
}

static int testTruncatedCheckpointFails(void) {
  int status = 0;
  int stepStatus = 0;
  int rc;

  runShell("rm -f run.out events.out run.restart *.log");

  stepStatus = prepRunFiles("restart_segment1.clim");
  if (stepStatus) {
    logTest("Failed to prepare files for truncation test segment 1\n");
    return stepStatus;
  }
  status |= (runModel("restart_seg1.in", "truncate_seg1.log") != 0);
  status |= truncateFileToSize(CHECKPOINT_FILE, 16);

  stepStatus = prepRunFiles("restart_segment2.clim");
  if (stepStatus) {
    logTest("Failed to prepare files for truncation test segment 2\n");
    return status | stepStatus;
  }

  rc = runModel("restart_seg2.in", "truncate_seg2.log");
  status |= (rc == 0);

  if (status) {
    logTest("testTruncatedCheckpointFails failed (rc=%d)\n", rc);
  }

  return status;
}

static int testMalformedCheckpointFails(void) {
  int status = 0;
  int stepStatus = 0;
  int rc;

  runShell("rm -f run.out events.out run.restart *.log");

  stepStatus = prepRunFiles("restart_segment1.clim");
  if (stepStatus) {
    logTest(
        "Failed to prepare files for malformed checkpoint test segment 1\n");
    return stepStatus;
  }
  status |= (runModel("restart_seg1.in", "malformed_seg1.log") != 0);
  status |=
      replaceFirstOccurrence(CHECKPOINT_FILE, "event_state", "event_state_BAD");

  stepStatus = prepRunFiles("restart_segment2.clim");
  if (stepStatus) {
    logTest(
        "Failed to prepare files for malformed checkpoint test segment 2\n");
    return status | stepStatus;
  }

  rc = runModel("restart_seg2.in", "malformed_seg2.log");
  status |= (rc == 0);

  if (status) {
    logTest("testMalformedCheckpointFails failed (rc=%d)\n", rc);
  }

  return status;
}

int run(void) {
  int status = 0;

  status |= testSegmentedEquivalence();
  status |= testStrictClimateMismatchFails();
  status |= testNoRestartModeUnchanged();
  status |= testModelVersionMismatchFails();
  status |= testSchemaMismatchFails();
  status |= testTruncatedCheckpointFails();
  status |= testMalformedCheckpointFails();

  return status;
}

int main(void) {
  int status;

  logTest("Starting testRestartMVP\n");
  status = run();
  if (status) {
    logTest("FAILED testRestartMVP with status %d\n", status);
    exit(status);
  }

  logTest("PASSED testRestartMVP\n");
  return 0;
}

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#include "common/logging.h"
#include "utils/tUtils.h"

#define SIPNET_CMD "../../../sipnet"
#define CHECKPOINT_FILE "run.restart"
#define RESTART_MAGIC_LINE "SIPNET_RESTART 1.0"
#define SCHEMA_LAYOUT_ENVI_LINE "schema_layout.envi_size 96"
#define SCHEMA_LAYOUT_TRACKERS_LINE "schema_layout.trackers_size 224"
#define SCHEMA_LAYOUT_PHENOLOGY_LINE "schema_layout.phenology_trackers_size 12"
#define SCHEMA_LAYOUT_EVENT_TRACKERS_LINE "schema_layout.event_trackers_size 8"

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

static int prepRunFiles(const char *climFile, const char *eventFile) {
  int status = 0;
  status |= copyFile((char *)"restart.param", (char *)"run.param");
  status |= copyFile((char *)climFile, (char *)"run.clim");
  status |= copyFile((char *)eventFile, (char *)"events.in");
  return status;
}

static int runModelWithArgs(const char *inputFile, const char *logFile,
                            const char *extraArgs) {
  char cmd[1024];
  if (extraArgs != NULL && extraArgs[0] != '\0') {
    sprintf(cmd, "%s -i %s %s > %s 2>&1", SIPNET_CMD, inputFile, extraArgs,
            logFile);
  } else {
    sprintf(cmd, "%s -i %s > %s 2>&1", SIPNET_CMD, inputFile, logFile);
  }
  return runShell(cmd);
}

static int runModel(const char *inputFile, const char *logFile) {
  return runModelWithArgs(inputFile, logFile, NULL);
}

static int truncateFileToSize(const char *file, long size) {
  if (truncate(file, size) != 0) {
    logTest("Unable to truncate %s to %ld bytes\n", file, size);
    return 1;
  }
  return 0;
}

static int stripFinalNewline(const char *file) {
  FILE *fp = fopen(file, "rb");
  if (fp == NULL) {
    logTest("Unable to open %s\n", file);
    return 1;
  }

  if (fseek(fp, 0, SEEK_END) != 0) {
    fclose(fp);
    return 1;
  }

  long size = ftell(fp);
  if (size <= 0) {
    fclose(fp);
    return 1;
  }

  if (fseek(fp, size - 1, SEEK_SET) != 0) {
    fclose(fp);
    return 1;
  }

  int last = fgetc(fp);
  fclose(fp);

  if (last == '\n') {
    return truncateFileToSize(file, size - 1);
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

static int replaceFirstLineStartingWith(const char *file, const char *prefix,
                                        const char *replacementLine) {
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

  char *pos = buffer;
  while ((pos = strstr(pos, prefix)) != NULL) {
    if (pos == buffer || *(pos - 1) == '\n') {
      break;
    }
    ++pos;
  }
  if (pos == NULL) {
    free(buffer);
    logTest("Could not find line starting with '%s' in %s\n", prefix, file);
    return 1;
  }

  char *lineEnd = strchr(pos, '\n');
  const char *afterLine =
      (lineEnd == NULL) ? (pos + strlen(pos)) : (lineEnd + 1);
  size_t beforeLen = (size_t)(pos - buffer);
  size_t replacementLen = strlen(replacementLine);
  size_t afterLen = strlen(afterLine);
  size_t newLen = beforeLen + replacementLen + afterLen;

  char *newContent = (char *)malloc(newLen + 1);
  if (newContent == NULL) {
    free(buffer);
    return 1;
  }

  memcpy(newContent, buffer, beforeLen);
  memcpy(newContent + beforeLen, replacementLine, replacementLen);
  memcpy(newContent + beforeLen + replacementLen, afterLine, afterLen);
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

static int fileContains(const char *file, const char *needle) {
  FILE *in = fopen(file, "r");
  if (in == NULL) {
    logTest("Unable to open %s\n", file);
    return 0;
  }

  char line[2048];
  int found = 0;
  while (fgets(line, sizeof(line), in) != NULL) {
    if (strstr(line, needle) != NULL) {
      found = 1;
      break;
    }
  }

  fclose(in);
  return found;
}

static int previousNonEmptyLineBeforeStartsWith(const char *file,
                                                const char *targetLine,
                                                const char *prefix) {
  FILE *in = fopen(file, "r");
  if (in == NULL) {
    logTest("Unable to open %s\n", file);
    return 0;
  }

  char line[2048];
  char prev[2048] = "";
  while (fgets(line, sizeof(line), in) != NULL) {
    line[strcspn(line, "\r\n")] = '\0';
    if (strcmp(line, targetLine) == 0) {
      fclose(in);
      return strncmp(prev, prefix, strlen(prefix)) == 0;
    }
    if (line[0] != '\0') {
      strcpy(prev, line);
    }
  }

  fclose(in);
  return 0;
}

static int testDefaultEventsFileUsedWhenUnset(void) {
  int status = 0;
  int stepStatus = 0;

  runShell("rm -f run.out events.out run.restart run.config custom_events.in "
           "*.log");

  stepStatus = prepRunFiles("restart_full.clim", "events_segment1.in");
  if (stepStatus) {
    logTest("Failed to prepare files for default-events-prefix test\n");
    return stepStatus;
  }
  status |= copyFile((char *)"events_segment2.in", (char *)"custom_events.in");

  status |= (runModel("restart_cont.in", "default_events_file.log") != 0);
  status |= !hasManagedEventOnDay("events.out", 2016, 47);
  status |= hasManagedEventOnDay("events.out", 2016, 49);

  if (status) {
    logTest("testDefaultEventsFileUsedWhenUnset failed\n");
  }

  return status;
}

static int testEventsPrefixCliOverrideUsed(void) {
  int status = 0;
  int stepStatus = 0;

  runShell("rm -f run.out events.out custom_events.out run.restart run.config "
           "custom_events.in *.log");

  stepStatus = prepRunFiles("restart_full.clim", "events_segment1.in");
  if (stepStatus) {
    logTest("Failed to prepare files for events-prefix CLI override test\n");
    return stepStatus;
  }
  status |= copyFile((char *)"events_segment2.in", (char *)"custom_events.in");

  status |= (runModelWithArgs("restart_cont.in", "events_prefix_cli.log",
                              "--events-prefix custom_events") != 0);
  status |= hasManagedEventOnDay("custom_events.out", 2016, 47);
  status |= !hasManagedEventOnDay("custom_events.out", 2016, 49);

  if (status) {
    logTest("testEventsPrefixCliOverrideUsed failed\n");
  }

  return status;
}

static int testConfigDumpIncludesRestartAndEventsKeys(void) {
  int status = 0;
  int stepStatus = 0;

  runShell("rm -f run.out events.out run.restart run.config *.log");

  stepStatus = prepRunFiles("restart_segment1.clim", "events_segment1.in");
  if (stepStatus) {
    logTest("Failed to prepare files for config-dump test\n");
    return stepStatus;
  }

  status |= (runModelWithArgs("restart_cont.in", "config_dump.log",
                              "--dump-config") != 0);
  status |= !fileContains("run.config", "RESTART_IN");
  status |= !fileContains("run.config", "RESTART_OUT");
  status |= !fileContains("run.config", "EVENTS_PREFIX");

  if (status) {
    logTest("testConfigDumpIncludesRestartAndEventsKeys failed\n");
  }

  return status;
}

static int testSegmentedEquivalence(void) {
  int status = 0;
  int stepStatus = 0;

  runShell("rm -f run.out events.out run.restart continuous.out seg1.out "
           "seg2.out segmented_joined.out *.log");

  stepStatus = prepRunFiles("restart_full.clim", "events_base.in");
  if (stepStatus) {
    logTest("Failed to prepare files for continuous run\n");
    return stepStatus;
  }

  status |= (runModel("restart_cont.in", "continuous.log") != 0);
  status |= rename("run.out", "continuous.out");
  status |= rename("events.out", "continuous.events");

  stepStatus = prepRunFiles("restart_segment1.clim", "events_segment1.in");
  if (stepStatus) {
    logTest("Failed to prepare files for segment 1\n");
    return status | stepStatus;
  }

  status |= (runModel("restart_seg1.in", "seg1.log") != 0);
  status |= !fileStartsWith(CHECKPOINT_FILE, RESTART_MAGIC_LINE);
  status |= !fileContains(CHECKPOINT_FILE, SCHEMA_LAYOUT_ENVI_LINE);
  status |= !fileContains(CHECKPOINT_FILE, SCHEMA_LAYOUT_TRACKERS_LINE);
  status |= !fileContains(CHECKPOINT_FILE, SCHEMA_LAYOUT_PHENOLOGY_LINE);
  status |= !fileContains(CHECKPOINT_FILE, SCHEMA_LAYOUT_EVENT_TRACKERS_LINE);
  status |= fileContains(CHECKPOINT_FILE, "boundary.tair ");
  status |= fileContains(CHECKPOINT_FILE, "boundary.tsoil ");
  status |= fileContains(CHECKPOINT_FILE, "boundary.par ");
  status |= fileContains(CHECKPOINT_FILE, "boundary.precip ");
  status |= fileContains(CHECKPOINT_FILE, "boundary.vpd ");
  status |= fileContains(CHECKPOINT_FILE, "boundary.vpdSoil ");
  status |= fileContains(CHECKPOINT_FILE, "boundary.vPress ");
  status |= fileContains(CHECKPOINT_FILE, "boundary.wspd ");
  status |= fileContains(CHECKPOINT_FILE, "event_state.");
  status |= !fileContains(CHECKPOINT_FILE, "trackers.gdd ");
  status |= fileContains(CHECKPOINT_FILE, "boundary.gdd");
  status |= fileContains(CHECKPOINT_FILE, "mean.length ");
  status |= fileContains(CHECKPOINT_FILE, "mean.totWeight ");
  status |= fileContains(CHECKPOINT_FILE, "mean.start ");
  status |= fileContains(CHECKPOINT_FILE, "mean.last ");
  status |= fileContains(CHECKPOINT_FILE, "mean.sum ");
  status |= fileContains(CHECKPOINT_FILE, "mean.values.length ");
  status |= fileContains(CHECKPOINT_FILE, "mean.weights.length ");
  status |= !fileContains(CHECKPOINT_FILE, "mean.npp.length ");
  status |= !fileContains(CHECKPOINT_FILE, "mean.npp.values.length ");
  status |= !fileContains(CHECKPOINT_FILE, "mean.npp.weights.length ");
  // These step-level diagnostics are intentionally omitted from restart schema.
  status |= fileContains(CHECKPOINT_FILE, "trackers.methane ");
  status |= fileContains(CHECKPOINT_FILE, "trackers.nLeaching ");
  status |= fileContains(CHECKPOINT_FILE, "trackers.nFixation ");
  status |= fileContains(CHECKPOINT_FILE, "trackers.nUptake ");
  status |= fileContains(CHECKPOINT_FILE, "balance.");
  status |= !previousNonEmptyLineBeforeStartsWith(
      CHECKPOINT_FILE, "end_restart 1", "mean.npp.weights.");
  status |= rename("run.out", "seg1.out");
  status |= rename("events.out", "seg1.events");

  stepStatus = prepRunFiles("restart_segment2.clim", "events_segment2.in");
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

  stepStatus = prepRunFiles("restart_segment1.clim", "events_segment1.in");
  if (stepStatus) {
    logTest("Failed to prepare files for mismatch test segment 1\n");
    return stepStatus;
  }

  status |= (runModel("restart_seg1.in", "mismatch_seg1.log") != 0);

  stepStatus = prepRunFiles("restart_segment2_bad.clim", "events_segment2.in");
  if (stepStatus) {
    logTest("Failed to prepare files for mismatch test segment 2\n");
    return status | stepStatus;
  }

  rc = runModel("restart_seg2_bad.in", "mismatch_seg2.log");
  status |= (rc != EXIT_CODE_BAD_PARAMETER_VALUE);

  if (status) {
    logTest("testStrictClimateMismatchFails failed (rc=%d)\n", rc);
  }

  return status;
}

static int testCheckpointFarFromMidnightWarnsAndWrites(void) {
  int status = 0;
  int stepStatus = 0;
  int rc;
  const char *warnInputFile = "restart_seg1_warn.in";

  runShell("rm -f run.out events.out run.restart *.log");

  stepStatus =
      prepRunFiles("restart_segment1_not_midnight.clim", "events_segment1.in");
  if (stepStatus) {
    logTest("Failed to prepare files for midnight-checkpoint test\n");
    return stepStatus;
  }

  status |= copyFile((char *)"restart_seg1.in", (char *)warnInputFile);
  status |= replaceFirstOccurrence(warnInputFile, "QUIET 1", "QUIET 0");

  rc = runModel(warnInputFile, "midnight_checkpoint.log");
  status |= (rc != 0);
  status |= !fileStartsWith(CHECKPOINT_FILE, RESTART_MAGIC_LINE);
  status |= !fileContains(
      "midnight_checkpoint.log",
      "last timestep ends more than one timestep before midnight");
  status |=
      !fileContains("midnight_checkpoint.log", "should not be used for resume");
  status |= (remove(warnInputFile) != 0);

  if (status) {
    logTest("testCheckpointFarFromMidnightWarnsAndWrites failed (rc=%d)\n", rc);
  }

  return status;
}

static int testTamperedBoundaryNotNearMidnightFails(void) {
  int status = 0;
  int stepStatus = 0;
  int rc;

  runShell("rm -f run.out events.out run.restart *.log");

  stepStatus = prepRunFiles("restart_segment1.clim", "events_segment1.in");
  if (stepStatus) {
    logTest("Failed to prepare files for tampered-boundary test segment 1\n");
    return stepStatus;
  }
  status |= (runModel("restart_seg1.in", "tampered_boundary_seg1.log") != 0);
  status |= replaceFirstLineStartingWith(CHECKPOINT_FILE, "boundary.time ",
                                         "boundary.time 12.0\n");

  stepStatus = prepRunFiles("restart_segment2.clim", "events_segment2.in");
  if (stepStatus) {
    logTest("Failed to prepare files for tampered-boundary test segment 2\n");
    return status | stepStatus;
  }

  rc = runModel("restart_seg2.in", "tampered_boundary_seg2.log");
  status |= (rc != EXIT_CODE_BAD_PARAMETER_VALUE);
  status |= !fileContains(
      "tampered_boundary_seg2.log",
      "checkpoint boundary is more than one timestep before midnight");

  if (status) {
    logTest("testTamperedBoundaryNotNearMidnightFails failed (rc=%d)\n", rc);
  }

  return status;
}

static int testRestartMustStartNearMidnight(void) {
  int status = 0;
  int stepStatus = 0;
  int rc;

  runShell("rm -f run.out events.out run.restart *.log");

  stepStatus = prepRunFiles("restart_segment1.clim", "events_segment1.in");
  if (stepStatus) {
    logTest("Failed to prepare files for restart-midnight test segment 1\n");
    return stepStatus;
  }
  status |= (runModel("restart_seg1.in", "restart_midnight_seg1.log") != 0);

  stepStatus = prepRunFiles("restart_segment2_late.clim", "events_segment2.in");
  if (stepStatus) {
    logTest("Failed to prepare files for restart-midnight test segment 2\n");
    return status | stepStatus;
  }

  rc = runModel("restart_seg2.in", "restart_midnight_seg2.log");
  status |= (rc != EXIT_CODE_BAD_PARAMETER_VALUE);
  status |= !fileContains("restart_midnight_seg2.log",
                          "must start within one timestep after midnight");

  if (status) {
    logTest("testRestartMustStartNearMidnight failed (rc=%d)\n", rc);
  }

  return status;
}

static int testRestartEventBoundaryRequiresSegmentedEvents(void) {
  int status = 0;
  int stepStatus = 0;
  int rc;

  runShell("rm -f run.out events.out run.restart *.log");

  stepStatus = prepRunFiles("restart_segment1.clim", "events_segment1.in");
  if (stepStatus) {
    logTest("Failed to prepare files for restart-event-boundary segment 1\n");
    return stepStatus;
  }
  status |=
      (runModel("restart_seg1.in", "restart_event_boundary_seg1.log") != 0);

  stepStatus = prepRunFiles("restart_segment2.clim", "events_base.in");
  if (stepStatus) {
    logTest("Failed to prepare files for restart-event-boundary segment 2\n");
    return status | stepStatus;
  }

  rc = runModel("restart_seg2.in", "restart_event_boundary_seg2.log");
  status |= (rc != EXIT_CODE_BAD_PARAMETER_VALUE);
  status |= !fileContains("restart_event_boundary_seg2.log",
                          "Restart event boundary mismatch");

  if (status) {
    logTest("testRestartEventBoundaryRequiresSegmentedEvents failed (rc=%d)\n",
            rc);
  }

  return status;
}

static int testMissingFinalNewlineCheckpointSucceeds(void) {
  int status = 0;
  int stepStatus = 0;
  int rc;

  runShell("rm -f run.out events.out run.restart *.log");

  stepStatus = prepRunFiles("restart_segment1.clim", "events_segment1.in");
  if (stepStatus) {
    logTest("Failed to prepare files for final-newline test segment 1\n");
    return stepStatus;
  }
  status |= (runModel("restart_seg1.in", "final_newline_seg1.log") != 0);
  status |= stripFinalNewline(CHECKPOINT_FILE);

  stepStatus = prepRunFiles("restart_segment2.clim", "events_segment2.in");
  if (stepStatus) {
    logTest("Failed to prepare files for final-newline test segment 2\n");
    return status | stepStatus;
  }

  rc = runModel("restart_seg2.in", "final_newline_seg2.log");
  status |= (rc != 0);

  if (status) {
    logTest("testMissingFinalNewlineCheckpointSucceeds failed (rc=%d)\n", rc);
  }

  return status;
}

static int testNoRestartModeUnchanged(void) {
  int status = 0;
  int stepStatus = 0;

  runShell("rm -f run.out events.out *.log no_restart_a.out no_restart_b.out");

  stepStatus = prepRunFiles("restart_full.clim", "events_base.in");
  if (stepStatus) {
    logTest("Failed to prepare files for no-restart A\n");
    return stepStatus;
  }
  status |= (runModel("norestart_a.in", "norestart_a.log") != 0);
  status |= rename("run.out", "no_restart_a.out");

  stepStatus = prepRunFiles("restart_full.clim", "events_base.in");
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

  stepStatus = prepRunFiles("restart_segment1.clim", "events_segment1.in");
  if (stepStatus) {
    logTest(
        "Failed to prepare files for model-version mismatch test segment 1\n");
    return stepStatus;
  }
  status |= (runModel("restart_seg1.in", "model_mismatch_seg1.log") != 0);
  status |= replaceFirstOccurrence(CHECKPOINT_FILE, "model_version ",
                                   "model_version X");

  stepStatus = prepRunFiles("restart_segment2.clim", "events_segment2.in");
  if (stepStatus) {
    logTest(
        "Failed to prepare files for model-version mismatch test segment 2\n");
    return status | stepStatus;
  }

  rc = runModel("restart_seg2.in", "model_mismatch_seg2.log");
  status |= (rc != EXIT_CODE_BAD_PARAMETER_VALUE);

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

  stepStatus = prepRunFiles("restart_segment1.clim", "events_segment1.in");
  if (stepStatus) {
    logTest("Failed to prepare files for schema mismatch test segment 1\n");
    return stepStatus;
  }
  status |= (runModel("restart_seg1.in", "schema_mismatch_seg1.log") != 0);
  status |= replaceFirstOccurrence(CHECKPOINT_FILE, "SIPNET_RESTART 1.0",
                                   "SIPNET_RESTART 9.9");

  stepStatus = prepRunFiles("restart_segment2.clim", "events_segment2.in");
  if (stepStatus) {
    logTest("Failed to prepare files for schema mismatch test segment 2\n");
    return status | stepStatus;
  }

  rc = runModel("restart_seg2.in", "schema_mismatch_seg2.log");
  status |= (rc != EXIT_CODE_BAD_PARAMETER_VALUE);

  if (status) {
    logTest("testSchemaMismatchFails failed (rc=%d)\n", rc);
  }

  return status;
}

static int testSchemaLayoutMismatchFails(void) {
  int status = 0;
  int stepStatus = 0;
  int rc;

  runShell("rm -f run.out events.out run.restart *.log");

  stepStatus = prepRunFiles("restart_segment1.clim", "events_segment1.in");
  if (stepStatus) {
    logTest("Failed to prepare files for schema-layout mismatch segment 1\n");
    return stepStatus;
  }
  status |=
      (runModel("restart_seg1.in", "schema_layout_mismatch_seg1.log") != 0);
  status |= replaceFirstOccurrence(CHECKPOINT_FILE, SCHEMA_LAYOUT_TRACKERS_LINE,
                                   "schema_layout.trackers_size 999");

  stepStatus = prepRunFiles("restart_segment2.clim", "events_segment2.in");
  if (stepStatus) {
    logTest("Failed to prepare files for schema-layout mismatch segment 2\n");
    return status | stepStatus;
  }

  rc = runModel("restart_seg2.in", "schema_layout_mismatch_seg2.log");
  status |= (rc != EXIT_CODE_BAD_PARAMETER_VALUE);
  status |= !fileContains("schema_layout_mismatch_seg2.log",
                          "Restart schema layout mismatch");

  if (status) {
    logTest("testSchemaLayoutMismatchFails failed (rc=%d)\n", rc);
  }

  return status;
}

static int testSchemaLayoutOverflowMismatchFails(void) {
  int status = 0;
  int stepStatus = 0;
  int rc;

  runShell("rm -f run.out events.out run.restart *.log");

  stepStatus = prepRunFiles("restart_segment1.clim", "events_segment1.in");
  if (stepStatus) {
    logTest(
        "Failed to prepare files for schema-layout overflow mismatch segment "
        "1\n");
    return stepStatus;
  }
  status |=
      (runModel("restart_seg1.in", "schema_layout_overflow_seg1.log") != 0);
  status |= replaceFirstOccurrence(CHECKPOINT_FILE, SCHEMA_LAYOUT_TRACKERS_LINE,
                                   "schema_layout.trackers_size 4294967520");

  stepStatus = prepRunFiles("restart_segment2.clim", "events_segment2.in");
  if (stepStatus) {
    logTest("Failed to prepare files for schema-layout overflow mismatch "
            "segment 2\n");
    return status | stepStatus;
  }

  rc = runModel("restart_seg2.in", "schema_layout_overflow_seg2.log");
  status |= (rc != EXIT_CODE_BAD_PARAMETER_VALUE);
  status |= !fileContains("schema_layout_overflow_seg2.log",
                          "Restart schema layout mismatch");

  if (status) {
    logTest("testSchemaLayoutOverflowMismatchFails failed (rc=%d)\n", rc);
  }

  return status;
}

static int testEndRestartMustBeOneFails(void) {
  int status = 0;
  int stepStatus = 0;
  int rc;

  runShell("rm -f run.out events.out run.restart *.log");

  stepStatus = prepRunFiles("restart_segment1.clim", "events_segment1.in");
  if (stepStatus) {
    logTest("Failed to prepare files for end_restart value test segment 1\n");
    return stepStatus;
  }
  status |= (runModel("restart_seg1.in", "end_restart_value_seg1.log") != 0);
  status |=
      replaceFirstOccurrence(CHECKPOINT_FILE, "end_restart 1", "end_restart 0");

  stepStatus = prepRunFiles("restart_segment2.clim", "events_segment2.in");
  if (stepStatus) {
    logTest("Failed to prepare files for end_restart value test segment 2\n");
    return status | stepStatus;
  }

  rc = runModel("restart_seg2.in", "end_restart_value_seg2.log");
  status |= (rc != EXIT_CODE_BAD_PARAMETER_VALUE);
  status |= !fileContains("end_restart_value_seg2.log",
                          "end_restart marker must be 1");

  if (status) {
    logTest("testEndRestartMustBeOneFails failed (rc=%d)\n", rc);
  }

  return status;
}

static int testNoKeysAllowedAfterEndRestartFails(void) {
  int status = 0;
  int stepStatus = 0;
  int rc;

  runShell("rm -f run.out events.out run.restart *.log");

  stepStatus = prepRunFiles("restart_segment1.clim", "events_segment1.in");
  if (stepStatus) {
    logTest("Failed to prepare files for post-end key test segment 1\n");
    return stepStatus;
  }
  status |= (runModel("restart_seg1.in", "post_end_key_seg1.log") != 0);
  status |= replaceFirstLineStartingWith(CHECKPOINT_FILE,
                                         SCHEMA_LAYOUT_ENVI_LINE, "");
  status |=
      replaceFirstOccurrence(CHECKPOINT_FILE, "end_restart 1",
                             "end_restart 1\n" SCHEMA_LAYOUT_ENVI_LINE "\n");

  stepStatus = prepRunFiles("restart_segment2.clim", "events_segment2.in");
  if (stepStatus) {
    logTest("Failed to prepare files for post-end key test segment 2\n");
    return status | stepStatus;
  }

  rc = runModel("restart_seg2.in", "post_end_key_seg2.log");
  status |= (rc != EXIT_CODE_BAD_PARAMETER_VALUE);
  status |= !fileContains("post_end_key_seg2.log",
                          "unexpected content after end_restart");

  if (status) {
    logTest("testNoKeysAllowedAfterEndRestartFails failed (rc=%d)\n", rc);
  }

  return status;
}

static int testMeanValueIndexOverflowFails(void) {
  int status = 0;
  int stepStatus = 0;
  int rc;

  runShell("rm -f run.out events.out run.restart *.log");

  stepStatus = prepRunFiles("restart_segment1.clim", "events_segment1.in");
  if (stepStatus) {
    logTest("Failed to prepare files for mean-index overflow test segment 1\n");
    return stepStatus;
  }
  status |= (runModel("restart_seg1.in", "mean_index_overflow_seg1.log") != 0);
  status |= replaceFirstOccurrence(CHECKPOINT_FILE, "mean.npp.values.0 ",
                                   "mean.npp.values.4294967296 ");

  stepStatus = prepRunFiles("restart_segment2.clim", "events_segment2.in");
  if (stepStatus) {
    logTest("Failed to prepare files for mean-index overflow test segment 2\n");
    return status | stepStatus;
  }

  rc = runModel("restart_seg2.in", "mean_index_overflow_seg2.log");
  status |= (rc != EXIT_CODE_BAD_PARAMETER_VALUE);
  status |= !fileContains("mean_index_overflow_seg2.log",
                          "invalid value '4294967296'");

  if (status) {
    logTest("testMeanValueIndexOverflowFails failed (rc=%d)\n", rc);
  }

  return status;
}

static int testNonFiniteRestartValuesFail(void) {
  int status = 0;
  int stepStatus = 0;
  int rc;

  runShell("rm -f run.out events.out run.restart *.log");

  stepStatus = prepRunFiles("restart_segment1.clim", "events_segment1.in");
  if (stepStatus) {
    logTest("Failed to prepare files for non-finite boundary test segment 1\n");
    return stepStatus;
  }
  status |= (runModel("restart_seg1.in", "nonfinite_boundary_seg1.log") != 0);
  status |= replaceFirstLineStartingWith(CHECKPOINT_FILE, "boundary.time ",
                                         "boundary.time nan\n");

  stepStatus = prepRunFiles("restart_segment2.clim", "events_segment2.in");
  if (stepStatus) {
    logTest("Failed to prepare files for non-finite boundary test segment 2\n");
    return status | stepStatus;
  }

  rc = runModel("restart_seg2.in", "nonfinite_boundary_seg2.log");
  status |= (rc != EXIT_CODE_BAD_PARAMETER_VALUE);
  status |= !fileContains("nonfinite_boundary_seg2.log",
                          "invalid value 'nan' for key 'boundary.time'");

  runShell("rm -f run.out events.out run.restart *.log");

  stepStatus = prepRunFiles("restart_segment1.clim", "events_segment1.in");
  if (stepStatus) {
    logTest("Failed to prepare files for non-finite mean test segment 1\n");
    return status | stepStatus;
  }
  status |= (runModel("restart_seg1.in", "nonfinite_mean_seg1.log") != 0);
  status |= replaceFirstLineStartingWith(CHECKPOINT_FILE, "mean.npp.sum ",
                                         "mean.npp.sum inf\n");

  stepStatus = prepRunFiles("restart_segment2.clim", "events_segment2.in");
  if (stepStatus) {
    logTest("Failed to prepare files for non-finite mean test segment 2\n");
    return status | stepStatus;
  }

  rc = runModel("restart_seg2.in", "nonfinite_mean_seg2.log");
  status |= (rc != EXIT_CODE_BAD_PARAMETER_VALUE);
  status |= !fileContains("nonfinite_mean_seg2.log",
                          "invalid value 'inf' for key 'mean.npp.sum'");

  if (status) {
    logTest("testNonFiniteRestartValuesFail failed\n");
  }

  return status;
}

static int testLegacyBalanceKeyRejected(void) {
  int status = 0;
  int stepStatus = 0;
  int rc;

  runShell("rm -f run.out events.out run.restart *.log");

  stepStatus = prepRunFiles("restart_segment1.clim", "events_segment1.in");
  if (stepStatus) {
    logTest("Failed to prepare files for legacy-balance test segment 1\n");
    return stepStatus;
  }
  status |= (runModel("restart_seg1.in", "legacy_balance_seg1.log") != 0);
  status |= replaceFirstOccurrence(CHECKPOINT_FILE, "end_restart 1",
                                   "balance.preTotalC 0\nend_restart 1");

  stepStatus = prepRunFiles("restart_segment2.clim", "events_segment2.in");
  if (stepStatus) {
    logTest("Failed to prepare files for legacy-balance test segment 2\n");
    return status | stepStatus;
  }

  rc = runModel("restart_seg2.in", "legacy_balance_seg2.log");
  status |= (rc != EXIT_CODE_BAD_PARAMETER_VALUE);
  status |= !fileContains("legacy_balance_seg2.log",
                          "unknown key 'balance.preTotalC'");

  if (status) {
    logTest("testLegacyBalanceKeyRejected failed (rc=%d)\n", rc);
  }

  return status;
}

static int testBuildInfoMismatchWarnsAndSucceeds(void) {
  int status = 0;
  int stepStatus = 0;
  int rc;
  const char *warnInputFile = "restart_seg2_build_warn.in";

  runShell("rm -f run.out events.out run.restart *.log");

  stepStatus = prepRunFiles("restart_segment1.clim", "events_segment1.in");
  if (stepStatus) {
    logTest("Failed to prepare files for build mismatch test segment 1\n");
    return stepStatus;
  }
  status |= (runModel("restart_seg1.in", "build_mismatch_seg1.log") != 0);
  status |=
      replaceFirstOccurrence(CHECKPOINT_FILE, "build_info ", "build_info X");

  stepStatus = prepRunFiles("restart_segment2.clim", "events_segment2.in");
  if (stepStatus) {
    logTest("Failed to prepare files for build mismatch test segment 2\n");
    return status | stepStatus;
  }

  status |= copyFile((char *)"restart_seg2.in", (char *)warnInputFile);
  status |= replaceFirstOccurrence(warnInputFile, "QUIET 1", "QUIET 0");

  rc = runModel(warnInputFile, "build_mismatch_seg2.log");
  status |= (rc != 0);
  status |=
      !fileContains("build_mismatch_seg2.log", "Restart build info mismatch");
  status |= (remove(warnInputFile) != 0);

  if (status) {
    logTest("testBuildInfoMismatchWarnsAndSucceeds failed (rc=%d)\n", rc);
  }

  return status;
}

static int testTruncatedCheckpointFails(void) {
  int status = 0;
  int stepStatus = 0;
  int rc;

  runShell("rm -f run.out events.out run.restart *.log");

  stepStatus = prepRunFiles("restart_segment1.clim", "events_segment1.in");
  if (stepStatus) {
    logTest("Failed to prepare files for truncation test segment 1\n");
    return stepStatus;
  }
  status |= (runModel("restart_seg1.in", "truncate_seg1.log") != 0);
  status |= truncateFileToSize(CHECKPOINT_FILE, 16);

  stepStatus = prepRunFiles("restart_segment2.clim", "events_segment2.in");
  if (stepStatus) {
    logTest("Failed to prepare files for truncation test segment 2\n");
    return status | stepStatus;
  }

  rc = runModel("restart_seg2.in", "truncate_seg2.log");
  status |= (rc != EXIT_CODE_BAD_PARAMETER_VALUE);

  if (status) {
    logTest("testTruncatedCheckpointFails failed (rc=%d)\n", rc);
  }

  return status;
}

static int testProcessedStepsOverflowFails(void) {
  int status = 0;
  int stepStatus = 0;
  int rc;

  runShell("rm -f run.out events.out run.restart *.log");

  stepStatus = prepRunFiles("restart_segment1.clim", "events_segment1.in");
  if (stepStatus) {
    logTest("Failed to prepare files for processed-steps overflow test "
            "segment 1\n");
    return stepStatus;
  }
  status |=
      (runModel("restart_seg1.in", "processed_steps_overflow_seg1.log") != 0);
  status |= replaceFirstLineStartingWith(
      CHECKPOINT_FILE, "processed_steps ",
      "processed_steps 999999999999999999999999999999\n");

  stepStatus = prepRunFiles("restart_segment2.clim", "events_segment2.in");
  if (stepStatus) {
    logTest("Failed to prepare files for processed-steps overflow test "
            "segment 2\n");
    return status | stepStatus;
  }

  rc = runModel("restart_seg2.in", "processed_steps_overflow_seg2.log");
  status |= (rc != EXIT_CODE_BAD_PARAMETER_VALUE);
  status |=
      !fileContains("processed_steps_overflow_seg2.log",
                    "invalid value '999999999999999999999999999999' for key "
                    "'processed_steps'");

  if (status) {
    logTest("testProcessedStepsOverflowFails failed (rc=%d)\n", rc);
  }

  return status;
}

static int testMalformedCheckpointFails(void) {
  int status = 0;
  int stepStatus = 0;
  int rc;

  runShell("rm -f run.out events.out run.restart *.log");

  stepStatus = prepRunFiles("restart_segment1.clim", "events_segment1.in");
  if (stepStatus) {
    logTest(
        "Failed to prepare files for malformed checkpoint test segment 1\n");
    return stepStatus;
  }
  status |= (runModel("restart_seg1.in", "malformed_seg1.log") != 0);
  status |= replaceFirstOccurrence(CHECKPOINT_FILE, "event_trackers",
                                   "event_trackers_BAD");

  stepStatus = prepRunFiles("restart_segment2.clim", "events_segment2.in");
  if (stepStatus) {
    logTest(
        "Failed to prepare files for malformed checkpoint test segment 2\n");
    return status | stepStatus;
  }

  rc = runModel("restart_seg2.in", "malformed_seg2.log");
  status |= (rc != EXIT_CODE_BAD_PARAMETER_VALUE);

  if (status) {
    logTest("testMalformedCheckpointFails failed (rc=%d)\n", rc);
  }

  return status;
}

int run(void) {
  int status = 0;

  status |= testDefaultEventsFileUsedWhenUnset();
  status |= testEventsPrefixCliOverrideUsed();
  status |= testConfigDumpIncludesRestartAndEventsKeys();
  status |= testSegmentedEquivalence();
  status |= testStrictClimateMismatchFails();
  status |= testCheckpointFarFromMidnightWarnsAndWrites();
  status |= testTamperedBoundaryNotNearMidnightFails();
  status |= testRestartMustStartNearMidnight();
  status |= testRestartEventBoundaryRequiresSegmentedEvents();
  status |= testMissingFinalNewlineCheckpointSucceeds();
  status |= testNoRestartModeUnchanged();
  status |= testModelVersionMismatchFails();
  status |= testSchemaMismatchFails();
  status |= testSchemaLayoutMismatchFails();
  status |= testSchemaLayoutOverflowMismatchFails();
  status |= testEndRestartMustBeOneFails();
  status |= testNoKeysAllowedAfterEndRestartFails();
  status |= testMeanValueIndexOverflowFails();
  status |= testNonFiniteRestartValuesFail();
  status |= testLegacyBalanceKeyRejected();
  status |= testBuildInfoMismatchWarnsAndSucceeds();
  status |= testTruncatedCheckpointFails();
  status |= testProcessedStepsOverflowFails();
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

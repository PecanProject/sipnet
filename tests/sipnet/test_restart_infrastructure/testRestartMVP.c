#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#include "common/logging.h"
#include "utils/tUtils.h"

#define CHECKPOINT_FILE "run.restart"
#define RESTART_MAGIC_LINE "SIPNET_RESTART 1.0"
#define MAX_LINE_LENGTH 1028

static int prepRunFiles(const char *climFile, const char *eventFile) {
  int status = 0;
  status |= copyFile((char *)"restart.param", (char *)"run.param");
  status |= copyFile((char *)climFile, (char *)"run.clim");
  status |= copyFile((char *)eventFile, (char *)"events.in");
  return status;
}

static int truncateFileToSize(const char *file, long size) {
  if (truncate(file, size) != 0) {
    logTest("Unable to truncate %s to %ld bytes\n", file, size);
    return 1;
  }
  return 0;
}

static int truncateFileToNLines(const char *file, int maxLines) {
  struct stat st;
  stat(file, &st);
  long size = st.st_size;
  if (size <= 0) {
    return 1;
  }
  if (maxLines < 0)
    return 1;

  FILE *fp = fopen(file, "r");
  if (fp == NULL) {
    logTest("Unable to open %s for reading\n", file);
    return 1;
  }

  int c;
  int lines = 0;
  long pos = 0;

  while ((c = fgetc(fp)) != EOF) {
    ++pos;

    if (c == '\n') {
      ++lines;
      if (lines == maxLines)
        break;
    }
  }

  /* If file has fewer lines than requested, do nothing */
  if (lines < maxLines) {
    fclose(fp);
    return 0;
  }

  fclose(fp);
  int result = truncateFileToSize(file, pos);

  return result;
}

static int stripFinalNewline(const char *file) {
  struct stat st;
  stat(file, &st);
  long size = st.st_size;
  if (size <= 0) {
    return 1;
  }

  FILE *fp = fopen(file, "rb");
  if (fp == NULL) {
    logTest("Unable to open %s\n", file);
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

static int replaceFirstLineStartingWith(const char *file, const char *prefix,
                                        const char *replacementLine) {
  struct stat st;
  stat(file, &st);
  long size = st.st_size;
  if (size <= 0) {
    return 1;
  }

  FILE *in = fopen(file, "r");
  if (in == NULL) {
    logTest("Unable to open %s for reading\n", file);
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

static int testDefaultEventsFileUsedWhenUnset(void) {
  int status = 0;
  int stepStatus = 0;

  logTest("Starting testDefaultEventsFileUsedWhenUnset\n");

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

  logTest("Starting testEventsPrefixCliOverrideUsed\n");

  runShell("rm -f run.out events.out custom_events.out run.restart run.config "
           "custom_events.in *.log");

  stepStatus = prepRunFiles("restart_full.clim", "events_segment1.in");
  if (stepStatus) {
    logTest("Failed to prepare files for events-prefix CLI override test\n");
    return stepStatus;
  }
  status |= copyFile("events_segment2.in", "custom_events.in");

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

  logTest("Starting testConfigDumpIncludesRestartAndEventsKeys\n");

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

  logTest("Starting testSegmentedEquivalence\n");

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

  if (status) {
    logTest("testSegmentedEquivalence failed\n");
  }

  return status;
}

static int testStrictClimateMismatchFails(void) {
  int status = 0;
  int stepStatus = 0;
  int rc;

  logTest("Starting testStrictClimateMismatchFails\n");

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
  status |= (rc != EXIT_CODE_BAD_RESTART_PARAMETER);

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

  logTest("Starting testCheckpointFarFromMidnightWarnsAndWrites\n");

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

// TODO: Decide if we are downgrading this to a warning
static int testTamperedBoundaryNotNearMidnightFails(void) {
  int status = 0;
  int stepStatus = 0;
  int rc;

  logTest("Starting testTamperedBoundaryNotNearMidnightFails\n");

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
  status |= (rc != EXIT_CODE_BAD_RESTART_PARAMETER);
  status |= !fileContains(
      "tampered_boundary_seg2.log",
      "checkpoint boundary is more than one timestep before midnight");

  if (status) {
    logTest("testTamperedBoundaryNotNearMidnightFails failed (rc=%d)\n", rc);
  }

  return status;
}

// TODO: Decide if we are downgrading this to a warning
static int testRestartMustStartNearMidnight(void) {
  int status = 0;
  int stepStatus = 0;
  int rc;

  logTest("Starting testRestartMustStartNearMidnight\n");

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
  status |= (rc != EXIT_CODE_BAD_RESTART_PARAMETER);
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

  logTest("Starting testRestartEventBoundaryRequiresSegmentedEvents\n");

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
  status |= (rc != EXIT_CODE_BAD_RESTART_PARAMETER);
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

  logTest("Starting testMissingFinalNewlineCheckpointSucceeds\n");

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

static int testModelVersionMismatchFails(void) {
  int status = 0;
  int stepStatus = 0;
  int rc;

  logTest("Starting testModelVersionMismatchFails\n");

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
  status |= (rc != EXIT_CODE_BAD_RESTART_PARAMETER);

  if (status) {
    logTest("testModelVersionMismatchFails failed (rc=%d)\n", rc);
  }

  return status;
}

static int testSchemaMismatchFails(void) {
  int status = 0;
  int stepStatus = 0;
  int rc;

  logTest("Starting testSchemaMismatchFails\n");

  runShell("rm -f run.out events.out run.restart *.log");

  stepStatus = prepRunFiles("restart_segment1.clim", "events_segment1.in");
  if (stepStatus) {
    logTest("Failed to prepare files for schema mismatch test segment 1\n");
    return stepStatus;
  }
  status |= (runModel("restart_seg1.in", "schema_mismatch_seg1.log") != 0);
  status |= replaceFirstOccurrence(CHECKPOINT_FILE, "SIPNET_RESTART ",
                                   "SIPNET_RESTART 1");

  stepStatus = prepRunFiles("restart_segment2.clim", "events_segment2.in");
  if (stepStatus) {
    logTest("Failed to prepare files for schema mismatch test segment 2\n");
    return status | stepStatus;
  }

  rc = runModel("restart_seg2.in", "schema_mismatch_seg2.log");
  status |= (rc != EXIT_CODE_BAD_RESTART_PARAMETER);

  if (status) {
    logTest("testSchemaMismatchFails failed (rc=%d)\n", rc);
  }

  return status;
}

static int testSchemaLayoutMismatchFails(void) {
  int status = 0;
  int stepStatus = 0;
  int rc;

  logTest("Starting testSchemaLayoutMismatchFails\n");

  runShell("rm -f run.out events.out run.restart *.log");

  stepStatus = prepRunFiles("restart_segment1.clim", "events_segment1.in");
  if (stepStatus) {
    logTest("Failed to prepare files for schema-layout mismatch segment 1\n");
    return stepStatus;
  }
  status |=
      (runModel("restart_seg1.in", "schema_layout_mismatch_seg1.log") != 0);
  status |=
      replaceFirstOccurrence(CHECKPOINT_FILE, "schema_layout.trackers_size ",
                             "schema_layout.trackers_size 1");

  stepStatus = prepRunFiles("restart_segment2.clim", "events_segment2.in");
  if (stepStatus) {
    logTest("Failed to prepare files for schema-layout mismatch segment 2\n");
    return status | stepStatus;
  }

  rc = runModel("restart_seg2.in", "schema_layout_mismatch_seg2.log");
  status |= (rc != EXIT_CODE_BAD_RESTART_PARAMETER);
  status |= !fileContains("schema_layout_mismatch_seg2.log",
                          "Restart schema layout mismatch");

  if (status) {
    logTest("testSchemaLayoutMismatchFails failed (rc=%d)\n", rc);
  }

  return status;
}

static int testMeanValueIndexOutOfRangeFails(void) {
  int status = 0;
  int stepStatus = 0;
  int rc;

  logTest("Starting testMeanValueIndexOutOfRangeFails\n");

  runShell("rm -f run.out events.out run.restart *.log");

  stepStatus = prepRunFiles("restart_segment1.clim", "events_segment1.in");
  if (stepStatus) {
    logTest(
        "Failed to prepare files for mean-index out of range test segment 1\n");
    return stepStatus;
  }
  status |=
      (runModel("restart_seg1.in", "mean_index_out_of_range_seg1.log") != 0);
  status |= replaceFirstOccurrence(CHECKPOINT_FILE, "mean.npp.values.0 ",
                                   "mean.npp.values.123456789 ");

  stepStatus = prepRunFiles("restart_segment2.clim", "events_segment2.in");
  if (stepStatus) {
    logTest(
        "Failed to prepare files for mean-index out of range test segment 2\n");
    return status | stepStatus;
  }

  rc = runModel("restart_seg2.in", "mean_index_out_of_range_seg2.log");
  status |= (rc != EXIT_CODE_BAD_RESTART_PARAMETER);
  status |= !fileContains("mean_index_out_of_range_seg2.log",
                          "index out of range (mean.npp.values.123456789)");

  if (status) {
    logTest("testMeanValueIndexOutOfRangeFails failed (rc=%d)\n", rc);
  }

  return status;
}

static int testNonFiniteRestartValuesFail(void) {
  int status = 0;
  int stepStatus = 0;
  int rc;

  logTest("Starting testNonFiniteRestartValuesFail\n");

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
  status |= (rc != EXIT_CODE_BAD_RESTART_PARAMETER);
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
  status |= (rc != EXIT_CODE_BAD_RESTART_PARAMETER);
  status |= !fileContains("nonfinite_mean_seg2.log",
                          "invalid value 'inf' for key 'mean.npp.sum'");

  if (status) {
    logTest("testNonFiniteRestartValuesFail failed\n");
  }

  return status;
}

static int testBuildInfoMismatchWarnsAndSucceeds(void) {
  int status = 0;
  int stepStatus = 0;
  int rc;
  const char *warnInputFile = "restart_seg2_build_warn.in";

  logTest("Starting testBuildInfoMismatchWarnsAndSucceeds\n");

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

  logTest("Starting testTruncatedCheckpointFails\n");

  runShell("rm -f run.out events.out run.restart *.log");

  stepStatus = prepRunFiles("restart_segment1.clim", "events_segment1.in");
  if (stepStatus) {
    logTest("Failed to prepare files for truncation test segment 1\n");
    return stepStatus;
  }
  status |= (runModel("restart_seg1.in", "truncate_seg1.log") != 0);
  status |= truncateFileToNLines(CHECKPOINT_FILE, 8);

  stepStatus = prepRunFiles("restart_segment2.clim", "events_segment2.in");
  if (stepStatus) {
    logTest("Failed to prepare files for truncation test segment 2\n");
    return status | stepStatus;
  }

  rc = runModel("restart_seg2.in", "truncate_seg2.log");
  status |= (rc != EXIT_CODE_BAD_RESTART_PARAMETER);
  status |= !fileContains("truncate_seg2.log", "missing required key");

  if (status) {
    logTest("testTruncatedCheckpointFails failed (rc=%d)\n", rc);
  }

  return status;
}

static int testMalformedCheckpointFails(void) {
  int status = 0;
  int stepStatus = 0;
  int rc;

  // This test checks for an unknown key in the restart file
  logTest("Starting testMalformedCheckpointFails\n");

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
  status |= (rc != EXIT_CODE_BAD_RESTART_PARAMETER);

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
  status |= testModelVersionMismatchFails();
  status |= testSchemaMismatchFails();
  status |= testSchemaLayoutMismatchFails();
  status |= testMeanValueIndexOutOfRangeFails();
  status |= testNonFiniteRestartValuesFail();
  status |= testBuildInfoMismatchWarnsAndSucceeds();
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

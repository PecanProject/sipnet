#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

#include "common/logging.h"
#include "utils/tUtils.h"

#define SIPNET_CMD "../../../sipnet"
#define CHECKPOINT_FILE "run.restart"

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

int run(void) {
  int status = 0;

  status |= testSegmentedEquivalence();
  status |= testStrictClimateMismatchFails();
  status |= testNoRestartModeUnchanged();

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

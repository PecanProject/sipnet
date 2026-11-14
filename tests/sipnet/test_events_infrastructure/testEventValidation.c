#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "common/context.h"
#include "common/exitCodes.h"
#include "common/logging.h"
#include "sipnet/events.h"
#include "sipnet/state.h"

enum Phenology { EXPLICIT_LEAVES, GDD, SOIL_PHENOL, LEAF_ON_DAY, LEAF_OFF_DAY };

typedef struct {
  const char *name;
  const char *records;
  int expectedCode;
} ReaderCase;

static void setPhenology(enum Phenology source) {
  updateIntContext("gdd", source == GDD, CTX_TEST);
  updateIntContext("soil-phenol", source == SOIL_PHENOL, CTX_TEST);
  params.leafOnDay = source == LEAF_ON_DAY ? 5 : 0;
  params.leafOffDay = source == LEAF_OFF_DAY ? 5 : 0;
}

static int writeRecords(const char *path, const char *records) {
  FILE *file = fopen(path, "w");
  if (file == NULL) {
    return 1;
  }
  int failed = fputs(records, file) == EOF;
  return fclose(file) != 0 || failed;
}

static int readAndCheck(const char *path, const char *records) {
  // Exercise the reader through the public event initialization interface.
  initEvents(path, "/dev/null", 0);
  setupEvents();
  EventNode *event = getCurrentEvent();
  int failed = 0;
  while (*records != '\0') {
    int year, day;
    char type[20];
    if (sscanf(records, "%d %d %19s", &year, &day, type) != 3 ||
        event == NULL || event->year != year || event->day != day ||
        strcmp(eventTypeToString(event->type), type) != 0) {
      failed = 1;
      break;
    }
    event = event->nextEvent;
    const char *end = strchr(records, '\n');
    records = end == NULL ? records + strlen(records) : end + 1;
  }
  failed |= event != NULL;
  freeEventList();
  closeEventOutFile();
  return failed;
}

static int runCase(const ReaderCase *test, enum Phenology source,
                   const char *secondRecords, enum Phenology secondSource) {
  char path[] = "/tmp/sipnet-event-validation-XXXXXX";
  int fd = mkstemp(path);
  if (fd < 0) {
    logTest("FAIL %s: cannot create event input\n", test->name);
    return 1;
  }
  close(fd);
  if (writeRecords(path, test->records)) {
    unlink(path);
    logTest("FAIL %s: cannot write event input\n", test->name);
    return 1;
  }

  // Use real exit codes, with fresh context and parameters for each case.
  fflush(NULL);
  pid_t child = fork();
  if (child == 0) {
    initContext();
    setPhenology(source);
    int failed = readAndCheck(path, test->records);
    if (!failed && secondRecords != NULL) {
      failed = writeRecords(path, secondRecords);
      if (!failed) {
        setPhenology(secondSource);
        failed = readAndCheck(path, secondRecords);
      }
    }
    freeContextMetadata();
    _exit(failed ? EXIT_CODE_FAILURE : EXIT_CODE_SUCCESS);
  }

  int status = 0;
  int waited = child > 0 && waitpid(child, &status, 0) == child;
  unlink(path);
  if (!waited || !WIFEXITED(status)) {
    logTest("FAIL %s: reader did not exit normally\n", test->name);
    return 1;
  }
  if (WEXITSTATUS(status) != test->expectedCode) {
    logTest("FAIL %s: exit %d, expected %d\n", test->name, WEXITSTATUS(status),
            test->expectedCode);
    return 1;
  }
  logTest("PASS %s\n", test->name);
  return 0;
}

int main(void) {
  int failed = 0;
  int count = 0;
  const char *sources[] = {"explicit", "gdd", "soil-phenol", "leafOnDay",
                           "leafOffDay"};
  const char *leaves[] = {"leafon", "leafoff"};
  const char *prefixes[] = {"", "2022 1 till 0.2\n",
                            "2022 1 till 0.2\n2022 2 irrig 1 0\n"};
  for (int source = GDD; source <= LEAF_OFF_DAY; ++source) {
    for (int leaf = 0; leaf < 2; ++leaf) {
      for (int position = 0; position < 3; ++position) {
        char name[100], records[160];
        snprintf(name, sizeof(name), "%s with %s in position %d",
                 sources[source], leaves[leaf], position + 1);
        snprintf(records, sizeof(records), "%s2022 5 %s\n", prefixes[position],
                 leaves[leaf]);
        ReaderCase test = {name, records, EXIT_CODE_BAD_PARAMETER_VALUE};
        failed += runCase(&test, source, NULL, EXPLICIT_LEAVES);
        ++count;
      }
    }
  }

  const ReaderCase cases[] = {
      {"plant then harvest on same day",
       "2022 5 plant 10 3 2 5\n2022 5 harv 0.1 0 0.9 0\n",
       EXIT_CODE_INPUT_FILE_ERROR},
      {"harvest then plant on same day",
       "2022 5 harv 0.1 0 0.9 0\n2022 5 plant 10 3 2 5\n",
       EXIT_CODE_INPUT_FILE_ERROR},
      {"plant and harvest with intervening tillage",
       "2022 5 plant 10 3 2 5\n2022 5 till 0.2\n2022 5 harv 0.1 0 0.9 0\n",
       EXIT_CODE_INPUT_FILE_ERROR},
      {"harvest and plant with intervening tillage",
       "2022 5 harv 0.1 0 0.9 0\n2022 5 till 0.2\n2022 5 plant 10 3 2 5\n",
       EXIT_CODE_INPUT_FILE_ERROR},
      {"plant and harvest after an earlier tillage record",
       "2022 1 till 0.2\n2022 5 plant 10 3 2 5\n2022 5 harv 0.1 0 0.9 0\n",
       EXIT_CODE_INPUT_FILE_ERROR},
      {"harvest and plant after an earlier tillage record",
       "2022 1 till 0.2\n2022 5 harv 0.1 0 0.9 0\n2022 5 plant 10 3 2 5\n",
       EXIT_CODE_INPUT_FILE_ERROR},
      {"same-day leaf-on must not clear planting flag",
       "2022 5 plant 10 3 2 5\n2022 5 leafon\n2022 5 harv 0.1 0 0.9 0\n",
       EXIT_CODE_INPUT_FILE_ERROR},
      {"same-day leaf-off must not clear harvest flag",
       "2022 5 harv 0.1 0 0.9 0\n2022 5 leafoff\n2022 5 plant 10 3 2 5\n",
       EXIT_CODE_INPUT_FILE_ERROR},
      {"explicit leaf timing", "2022 1 leafon\n2022 5 leafoff\n",
       EXIT_CODE_SUCCESS},
      {"explicit leaf timing after management",
       "2022 1 till 0.2\n2022 2 leafon\n2022 5 leafoff\n", EXIT_CODE_SUCCESS},
      {"plant and harvest on different days",
       "2022 1 plant 10 3 2 5\n2022 2 harv 0.1 0 0.9 0\n", EXIT_CODE_SUCCESS},
      {"harvest and plant on different days",
       "2022 1 harv 0.1 0 0.9 0\n2022 2 plant 10 3 2 5\n", EXIT_CODE_SUCCESS},
      {"leaf-on starts a new day after planting",
       "2022 1 plant 10 3 2 5\n2022 2 leafon\n2022 2 harv 0.1 0 0.9 0\n",
       EXIT_CODE_SUCCESS},
      {"leaf-off starts a new day after planting",
       "2022 1 plant 10 3 2 5\n2022 2 leafoff\n2022 2 harv 0.1 0 0.9 0\n",
       EXIT_CODE_SUCCESS},
      {"leaf-on starts a new day after harvest",
       "2022 1 harv 0.1 0 0.9 0\n2022 2 leafon\n2022 2 plant 10 3 2 5\n",
       EXIT_CODE_SUCCESS},
      {"leaf-off starts a new day after harvest",
       "2022 1 harv 0.1 0 0.9 0\n2022 2 leafoff\n2022 2 plant 10 3 2 5\n",
       EXIT_CODE_SUCCESS},
      {"leaf-on starts a new year after planting",
       "2022 365 plant 10 3 2 5\n2023 1 leafon\n2023 1 harv 0.1 0 0.9 0\n",
       EXIT_CODE_SUCCESS},
      {"leaf-off starts a new year after harvest",
       "2022 365 harv 0.1 0 0.9 0\n2023 1 leafoff\n2023 1 plant 10 3 2 5\n",
       EXIT_CODE_SUCCESS},
      {"plant and harvest on same day number in different years",
       "2022 5 plant 10 3 2 5\n2023 5 harv 0.1 0 0.9 0\n", EXIT_CODE_SUCCESS},
      {"later malformed record", "2022 1 till 0.2\n2022 invalid\n",
       EXIT_CODE_INPUT_FILE_ERROR},
      {"later unknown event", "2022 1 till 0.2\n2022 2 unknown\n",
       EXIT_CODE_UNKNOWN_EVENT_TYPE_OR_PARAM},
      {"later malformed parameters", "2022 1 till 0.2\n2022 2 till invalid\n",
       EXIT_CODE_INPUT_FILE_ERROR},
      {"later invalid harvest fractions",
       "2022 1 till 0.2\n2022 2 harv 0.9 0 0.9 0\n",
       EXIT_CODE_BAD_PARAMETER_VALUE},
      {"later out-of-order day", "2022 5 till 0.2\n2022 1 till 0.2\n",
       EXIT_CODE_INPUT_FILE_ERROR},
      {"later out-of-order year", "2022 1 till 0.2\n2021 5 till 0.2\n",
       EXIT_CODE_INPUT_FILE_ERROR},
  };
  for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
    failed += runCase(&cases[i], EXPLICIT_LEAVES, NULL, EXPLICIT_LEAVES);
    ++count;
  }

  const ReaderCase plantingFile = {"daily flags reset between files",
                                   "2022 5 plant 10 3 2 5\n",
                                   EXIT_CODE_SUCCESS};
  failed += runCase(&plantingFile, EXPLICIT_LEAVES, "2022 5 harv 0.1 0 0.9 0\n",
                    EXPLICIT_LEAVES);
  ++count;
  const ReaderCase leafFile = {"leaf compatibility check resets between files",
                               "2022 5 leafon\n",
                               EXIT_CODE_BAD_PARAMETER_VALUE};
  failed += runCase(&leafFile, EXPLICIT_LEAVES, "2022 5 leafoff\n", GDD);
  ++count;

  logTest("testEventValidation: %d/%d cases passed\n", count - failed, count);
  return failed ? EXIT_CODE_FAILURE : EXIT_CODE_SUCCESS;
}

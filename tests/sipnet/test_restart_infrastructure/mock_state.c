#include "utils/tUtils.h"
#include "common/context.h"

#define STATE_FILE "../../../src/sipnet/state.h"
#define CONTEXT_FILE "../../../src/common/context.h"
#define BAD_STATE "bad_code/state.h.envi"
#define BAD_CTX "bad_code/context.h.flag"

int main(void) {
  // Generate a restart file from current sipnet
  copyFile("restart_segment1.clim", "restart.clim");
  runModelWithArgs("restart_seg1.in", "restart.log",
                   "-e events_segment1 -f restart");

  // Missed envi update
  copyFile(STATE_FILE, BAD_STATE);
  replaceFirstOccurrence(BAD_STATE, "double plantWoodCStorageDelta;",
                         "double plantWoodCStorageDelta;double dummyPool;");

  // Missed context update
  copyFile(CONTEXT_FILE, BAD_CTX);
  char old[36];
  char new[36];
  sprintf(old, "%s %d", "#define NUM_CONTEXT_MODEL_FLAGS",
          NUM_CONTEXT_MODEL_FLAGS);
  sprintf(new, "%s %d", "#define NUM_CONTEXT_MODEL_FLAGS",
          NUM_CONTEXT_MODEL_FLAGS + 1);
  replaceFirstOccurrence(BAD_CTX, old, new);
  replaceFirstOccurrence(BAD_CTX, "int anaerobic;",
                         "int anaerobic;int dummyFlag;");

  return 0;
}

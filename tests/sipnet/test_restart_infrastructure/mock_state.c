#include "utils/tUtils.h"
#include "common/context.h"

#define STATE_FILE "../../../src/sipnet/state.h"
#define CONTEXT_FILE "../../../src/common/context.h"

int main(void) {
  // Generate a restart file from current sipnet
  copyFile("restart_segment1.clim", "restart.clim");
  runModelWithArgs("restart_seg1.in", "restart.log",
                   "-e events_segment1 -f restart");

  // Missed envi update
  copyFile(STATE_FILE, "state.h.envi");
  replaceFirstOccurrence("state.h.envi", "double plantWoodCStorageDelta;",
                         "double plantWoodCStorageDelta;double dummyPool;");

  // Missed context update
  copyFile(CONTEXT_FILE, "context.h.flag");
  char old[36];
  char new[36];
  sprintf(old, "%s %d", "#define NUM_CONTEXT_MODEL_FLAGS",
          NUM_CONTEXT_MODEL_FLAGS);
  sprintf(new, "%s %d", "#define NUM_CONTEXT_MODEL_FLAGS",
          NUM_CONTEXT_MODEL_FLAGS + 1);
  replaceFirstOccurrence("context.h.flag", old, new);
  replaceFirstOccurrence("context.h.flag", "int anaerobic;",
                         "int anaerobic;int dummyFlag;");

  return 0;
}

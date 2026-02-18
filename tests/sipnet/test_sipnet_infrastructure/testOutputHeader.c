#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "common/logging.h"
#include "sipnet/sipnet.c"

#define MAX_LINE_LEN 4096
#define MAX_TOKENS 1024
#define FORMAT_OUTPUT_FILE "format_check.out"

/* Extract end indices of non-space tokens from a line.
   Returns number of tokens found. */
int extract_token_end_indices(const char *line, int *end_indices) {
  int count = 0;
  int in_token = 0;

  for (int i = 0; line[i] != '\0' && line[i] != '\n'; i++) {
    if (!isspace((unsigned char)line[i])) {
      in_token = 1;
    } else {
      if (in_token) {
        end_indices[count++] = i - 1;
        in_token = 0;
      }
    }
  }

  /* Handle token ending at end of line */
  if (in_token) {
    int len = strlen(line);
    if (len > 0 && line[len - 1] == '\n') {
      len--;
    }
    end_indices[count++] = len - 1;
  }

  return count;
}

int checkOutput(void) {
  int status = 0;

  FILE *fp = fopen(FORMAT_OUTPUT_FILE, "r");
  if (!fp) {
    logTest("Output file failed to open for read, exiting\n");
    return 1;
  }

  char line1[MAX_LINE_LEN];
  char line2[MAX_LINE_LEN];

  if (!fgets(line1, sizeof(line1), fp) || !fgets(line2, sizeof(line2), fp)) {
    logTest("Output file does not have at least two lines, exiting\n");
    fclose(fp);
    return 1;
  }

  fclose(fp);

  int ends1[MAX_TOKENS];
  int ends2[MAX_TOKENS];

  int count1 = extract_token_end_indices(line1, ends1);
  int count2 = extract_token_end_indices(line2, ends2);

  if (count1 != count2) {
    logTest("Mismatch: different number of tokens (%d vs %d)\n", count1,
            count2);
    return 1;
  }

  for (int i = 0; i < count1; i++) {
    if (ends1[i] != ends2[i]) {
      logTest("Token %d mismatch: line1 ends at %d, line2 ends at %d\n", i + 1,
              ends1[i], ends2[i]);
      status = 1;
    }
  }

  return status;
}

void init(void) { initTrackers(); }

void genOutput(FILE *out) {
  init();

  outputHeader(out);
  outputState(out, 2026, 50, 0.0);
}

int run(void) {
  int status = 0;

  FILE *out = openFile(FORMAT_OUTPUT_FILE, "w");
  if (!out) {
    logTest("Output file failed to open for write, exiting\n");
    return 1;
  }

  genOutput(out);
  fclose(out);

  status |= checkOutput();

  return status;
}

int main(void) {
  int status;

  logTest("Starting test testOutputHeader\n");

  status = run();
  if (status) {
    logTest("FAILED testOutputHeader with status %d\n", status);
    exit(status);
  }

  logTest("PASSED testOutputHeader\n");
}

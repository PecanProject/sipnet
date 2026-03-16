#ifndef UTILS_H
#define UTILS_H

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include "common/exitCodes.h"
#include "common/logging.h"

// This should be the correct relative path for where the tests are, not
// for this file
#define SIPNET_CMD "../../../sipnet"

extern inline int copyFile(const char *src, const char *dest) {
  FILE *source = fopen(src, "rb");  // Binary mode for compatibility
  if (source == NULL) {
    printf("Error opening source file %s", src);
    return 1;
  }

  FILE *destination = fopen(dest, "wb");
  if (destination == NULL) {
    printf("Error opening destination file %s", dest);
    fclose(source);
    return 1;
  }

  char buffer[4096];  // 4KB buffer
  size_t bytesRead;

  while ((bytesRead = fread(buffer, 1, sizeof(buffer), source)) > 0) {
    if (ferror(source)) {
      printf("Error reading file %s during file copy\n", src);
      exit(EXIT_CODE_FILE_OPEN_OR_READ_ERROR);
    }
    fwrite(buffer, 1, bytesRead, destination);
    if (feof(source)) {
      break;
    }
  }

  fclose(source);
  fclose(destination);
  return 0;
}

extern inline int compareDoubles(double a, double b) {
  return fabs(a - b) < 1e-6;
}

extern inline int diffFiles(const char *fname1, const char *fname2) {
  FILE *file2 = fopen(fname1, "rb");
  FILE *file1 = fopen(fname2, "rb");

  if (file1 == NULL || file2 == NULL) {
    printf("Error opening files\n");
    if (file1) {
      fclose(file1);
    }
    if (file2) {
      fclose(file2);
    }
    return 1;
  }

  int char1, char2;
  int status = 0;

  while (1) {
    char1 = fgetc(file1);
    char2 = fgetc(file2);

    if (char1 == EOF && char2 == EOF) {
      break;
    } else if (char1 == EOF || char2 == EOF || char1 != char2) {
      char command[80];
      sprintf(command, "diff %s %s", fname1, fname2);
      system(command);
      status = 1;
      break;
    }
  }

  fclose(file1);
  fclose(file2);

  return status;
}

extern inline int runShell(const char *cmd) {
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

extern inline int runModelWithArgs(const char *inputFile, const char *logFile,
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

extern inline int runModel(const char *inputFile, const char *logFile) {
  return runModelWithArgs(inputFile, logFile, NULL);
}

extern inline int fileContains(const char *file, const char *needle) {
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

extern inline int replaceFirstOccurrence(const char *file, const char *needle,
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

extern inline int truncateFileToSize(const char *file, long size) {
  if (truncate(file, size) != 0) {
    logTest("Unable to truncate %s to %ld bytes\n", file, size);
    return 1;
  }
  return 0;
}

extern inline int getFileSize(const char *file, long *size) {
  int status = 0;
  struct stat st;
  if (stat(file, &st)) {
    // stat error
    logTest("Error in stat() for %s\n", file);
    status = 1;
  } else {
    *size = st.st_size;
    if (*size <= 0) {
      status = 1;
    }
  }

  return status;
}
extern inline int truncateFileToNLines(const char *file, int maxLines) {
  long size;
  int status = getFileSize(file, &size);
  if (status) {
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

#endif  // UTILS_H

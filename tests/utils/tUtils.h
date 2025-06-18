#ifndef UTILS_H
#define UTILS_H

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "common/exitCodes.h"

extern inline int copyFile(char *src, char *dest) {
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

#endif  // UTILS_H

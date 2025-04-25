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

#endif  // UTILS_H

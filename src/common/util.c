/* Basic utility functions

   Author: Bill Sacks

   Creation date: 8/17/04
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#include "exitCodes.h"
#include "logging.h"
#include "util.h"

// our own openFile method, which exits gracefully if there's an error
FILE *openFile(const char *name, const char *mode) {
  FILE *f;

  if ((f = fopen(name, mode)) == NULL) {
    const char *mode_word =
        (!strcmp(mode, "r") || !strcmp(mode, "rb")) ? "reading" : "writing";
    fprintf(stderr, "Error %s '%s': %s\n", mode_word, name, strerror(errno));
    exit(EXIT_CODE_FILE_OPEN_OR_READ_ERROR);
  }

  return f;
}

// If line contains any character in the string commentChars,
//  strip the comment off the line (i.e. replace first occurrence of
//  commentChars with '\0')
// Return 1 if line contains only a comment (or only blanks), 0 otherwise
int stripComment(char *line, const char *commentChars) {
  char *commentCharLoc;
  int lenTrim;

  // strip trailing comment:
  commentCharLoc = strpbrk(line, commentChars);
  if (commentCharLoc != NULL) {
    commentCharLoc[0] = '\0';
  }

  // determine length without any leading blanks
  lenTrim = strlen(line) - strspn(line, " \t\n\r");
  return (lenTrim == 0);
}

// count number of fields in a string separated by delimiter 'sep'
int countFields(const char *line, const char *sep) {
  // strtok modifies string, so we need a copy
  size_t lineLen = strlen(line);
  char *lineCopy = (char *)malloc(lineLen + 1);
  if (lineCopy == NULL) {
    logError("memory allocation failure in file processing\n");
    exit(EXIT_CODE_INTERNAL_ERROR);
  }
  strcpy(lineCopy, line);

  int numParams = 0;
  char *par = strtok(lineCopy, sep);
  while (par != NULL) {
    ++numParams;
    par = strtok(NULL, sep);
  }
  free(lineCopy);
  return numParams;
}

double calcRatio(const double num, const double den) {
  const double effectiveDen = den < TINY ? TINY : den;
  return num / effectiveDen;
}

// For global linkage
extern inline double unitClip(double preClip);

DynamicString *dsCreate(size_t initial_capacity) {
  DynamicString *ds = malloc(sizeof(DynamicString));
  if (!ds)
    return NULL;

  // Ensure we have room for at least a null terminator
  ds->capacity = (initial_capacity > 0) ? initial_capacity : 16;
  ds->buffer = malloc(ds->capacity * sizeof(char));

  if (!ds->buffer) {
    free(ds);
    return NULL;
  }

  ds->buffer[0] = '\0';  // Start with an empty string
  ds->length = 0;
  return ds;
}

int dsAppend(DynamicString *ds, const char *str) {
  if (!ds || !str)
    return 0;

  size_t append_len = strlen(str);
  // +1 is crucial to ensure room for the null terminator
  size_t needed_capacity = ds->length + append_len + 1;

  // Double capacity until it's large enough for the new content
  if (needed_capacity > ds->capacity) {
    size_t new_capacity = ds->capacity * 2;
    while (new_capacity < needed_capacity) {
      new_capacity *= 2;
    }

    // Safely reallocate using a temporary pointer
    char *temp = realloc(ds->buffer, new_capacity * sizeof(char));
    if (!temp) {
      return 0;  // Reallocation failed, original data remains intact
    }

    ds->buffer = temp;
    ds->capacity = new_capacity;
  }

  // Copy the new string over the old null terminator
  memcpy(ds->buffer + ds->length, str, append_len);
  ds->length += append_len;
  ds->buffer[ds->length] = '\0';  // Manually place the new null terminator

  return 1;  // Success
}

int dsAppendFormatted(DynamicString *ds, const char *format, ...) {
  if (!ds || !format)
    return 0;

  // 1. Determine how much space the formatted text needs
  va_list args;
  va_start(args, format);
  // Make a copy of args because vsnprintf consumes the list
  va_list args_copy;
  va_copy(args_copy, args);

  // Pass NULL and 0 to just count the characters needed
  int formatted_len = vsnprintf(NULL, 0, format, args_copy);
  va_end(args_copy);

  if (formatted_len < 0) {
    va_end(args);
    return 0;  // Formatting error occurred
  }

  // 2. Ensure the buffer is big enough
  size_t needed_capacity = ds->length + (size_t)formatted_len + 1;
  if (needed_capacity > ds->capacity) {
    size_t new_capacity = ds->capacity * 2;
    while (new_capacity < needed_capacity) {
      new_capacity *= 2;
    }

    char *temp = realloc(ds->buffer, new_capacity * sizeof(char));
    if (!temp) {
      va_end(args);
      return 0;  // Reallocation failed
    }
    ds->buffer = temp;
    ds->capacity = new_capacity;
  }

  // 3. Write the formatted string directly into the builder's buffer
  // Write directly to the position of the current null terminator
  vsnprintf(ds->buffer + ds->length, (size_t)formatted_len + 1, format, args);
  va_end(args);

  // 4. Update the length of the string builder
  ds->length += (size_t)formatted_len;
  return 1;
}

void dsFree(DynamicString *ds) {
  if (ds) {
    free(ds->buffer);
    free(ds);
  }
}

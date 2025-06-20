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
#include "util.h"

// TODO: HOW TO INCLUDE CTX - move warn/info to context.h|c?

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

// do an strcmp on s1 and s2, ignoring case
// (convert both to lower case before comparing)
// return value is the same as for strcmp
int strcmpIgnoreCase(const char *s1, const char *s2) {
  char *s1Lower;
  char *s2Lower;
  int i;
  int result;

  // allocate space for copies
  // note that we need one more than strlen to allow room for termination
  // character
  s1Lower = (char *)malloc((strlen(s1) + 1) * sizeof(char));
  s2Lower = (char *)malloc((strlen(s2) + 1) * sizeof(char));

  for (i = 0; i <= strlen(s1); i++) {
    s1Lower[i] = tolower(s1[i]);
  }
  for (i = 0; i <= strlen(s2); i++) {
    s2Lower[i] = tolower(s2[i]);
  }

  result = strcmp(s1Lower, s2Lower);

  free(s1Lower);
  free(s2Lower);

  return result;
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
  char lineCopy[256];
  strcpy(lineCopy, line);
  int numParams = 0;
  char *par = strtok(lineCopy, sep);
  while (par != NULL) {
    ++numParams;
    par = strtok(NULL, sep);
  }
  return numParams;
}

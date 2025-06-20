/* Basic utility functions

   Author: Bill Sacks

   Creation date: 8/17/04
*/

#ifndef UTIL_H
#define UTIL_H

#include <stdio.h>

// our own openFile method, which exits gracefully if there's an error
FILE *openFile(const char *name, const char *mode);

// do an strcmp on s1 and s2, ignoring case
// (convert both to lower case before comparing)
// return value is the same as for strcmp
int strcmpIgnoreCase(const char *s1, const char *s2);

// If line contains any character in the string commentChars,
//  strip the comment off the line (i.e. replace first occurrence of
//  commentChars with '\0')
// Return 1 if line contains only a comment (or only blanks), 0 otherwise
int stripComment(char *line, const char *commentChars);

int countFields(const char *line, const char *sep);

#endif

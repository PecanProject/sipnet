/* Basic utility functions

   Author: Bill Sacks

   Creation date: 8/17/04
*/

#ifndef UTIL_H
#define UTIL_H

#include <math.h>
#include <stdio.h>

#define TINY 0.000001  // to avoid those nasty divide-by-zero errors

// our own openFile method, which exits gracefully if there's an error
FILE *openFile(const char *name, const char *mode);

// If line contains any character in the string commentChars,
//  strip the comment off the line (i.e. replace first occurrence of
//  commentChars with '\0')
// Return 1 if line contains only a comment (or only blanks), 0 otherwise
int stripComment(char *line, const char *commentChars);

int countFields(const char *line, const char *sep);

/**
 * Calculate the ratio of the inputs safely
 */
double calcRatio(double num, double den);

/**
 * Clips input double to [0,1]
 *
 * @param preClip Pre-clipped value
 * @return Clipped value
 */
inline double unitClip(double preClip) { return fmin(fmax(preClip, 0.0), 1.0); }

#endif

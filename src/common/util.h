/* Basic utility functions

   Author: Bill Sacks

   Creation date: 8/17/04
*/

#ifndef UTIL_H
#define UTIL_H

#include <stdio.h>

// set filename = <base>.<ext>
// assumes filename has been allocated and is large enough to hold result
void buildFileName(char *filename, const char *base, const char *ext);

// our own openFile method, which exits gracefully if there's an error
FILE *openFile(const char *name, const char *mode);

// call openFile on a file with name <name>.<ext> (i.e. include an extension)
FILE *openFileExt(const char *name, const char *ext, const char *mode);

// if seed = 0, seed based on time, otherwise seed based on seed
// if outF is specified (not NULL), write seed to file
void seedRand(unsigned int seed, FILE *outF);

// returns an exponentially-distributed negative random number
double randm(void);

// allocate space for an array of doubles of given size,
// return pointer to start of array
double *makeArray(int size);

/* Dynamically allocate space for a 2-d array of doubles of given size,
   return pointer to start of array

   Ensures that array elements are continuous to increase efficiency

   To free array, first free(arr[0]), then free(arr)
*/
double **make2DArray(int nrows, int ncols);

// same as make2DArray, but for ints
int **make2DIntArray(int nrows, int ncols);

// free a 2D Array that was created using make2DArray
// pre: arr has been cast to (void **)
void free2DArray(void **arr);

// set each element of out[0..n-1] to corresponding element of in[0..n-1]
void assignArray(double *out, double *in, int n);

// return sum(array[0..length-1])
double sumArray(double *array, int length);

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

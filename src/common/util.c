/* Basic utility functions

   Author: Bill Sacks

   Creation date: 8/17/04
*/

#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <ctype.h>
#include "util.h"


// set filename = <base>.<ext>
// assumes filename has been allocated and is large enough to hold result
void buildFileName(char *filename, const char *base, const char *ext)  {
  strcpy(filename, base);
  strcat(filename, ".");
  strcat(filename, ext);
}


// our own openFile method, which exits gracefully if there's an error
FILE *openFile(const char *name, const char *mode) {
  FILE *f;

  if ((f = fopen(name, mode)) == NULL) {
    printf("Error opening %s for %s\n", name, mode);
    exit(1);
  }

  return f;
}


// call openFile on a file with name <name>.<ext> (i.e. include an extension)
FILE *openFileExt(const char *name, const char *ext, const char *mode)  {
  char *fullName;
  FILE *f;
  
  fullName = (char *)malloc((strlen(name) + strlen(ext) + 2) * sizeof(char));
  buildFileName(fullName, name, ext);
  
  f = openFile(fullName, mode);

  free(fullName);
  return f;
}


// if seed = 0, seed based on time, otherwise seed based on seed
// if outF is specified (not NULL), write seed to file
void seedRand(unsigned int seed, FILE *outF) {
  time_t t;

  if (seed == 0) {
    t = time(NULL);
    if (outF != NULL)
      fprintf(outF, "Seeding random number generator with time: %d\n\n", (unsigned int)t);
    srand(t);
  }
  else {
    if (outF != NULL)
      fprintf(outF, "Seeding random number generator with %d\n\n", seed);
    srand(seed);
  }
}


// returns an exponentially-distributed negative random number 
double randm() {
  double val;

  do {
    val = rand()/(RAND_MAX + 1.0);
  } while (val <= 0.0); // val should be <= 0.0 once in a blue moon
  return log(val);
}

// allocate space for an array of doubles of given size,
// return pointer to start of array
double *makeArray(int size) {
  double *ptr;
  ptr = (double *)malloc(size * sizeof(double));
  return ptr;
}


/* Dynamically allocate space for a 2-d array of doubles of given size,
   return pointer to start of array
   
   Ensures that array elements are continuous to increase efficiency

   To free array, first free(arr[0]), then free(arr)
*/
double **make2DArray(int nrows, int ncols) {
  double **arr;
  int i;

  arr = (double **)malloc(nrows * sizeof(double *));

  // assure that all space is allocated continuously: 
  arr[0] = (double *)malloc(nrows * ncols * sizeof(double));
  for (i = 1; i < nrows; i++)
    arr[i] = arr[0] + i*ncols;

  return arr;
}

// same as make2DArray, but for ints
int **make2DIntArray(int nrows, int ncols) {
  int **arr;
  int i;

  arr = (int **)malloc(nrows * sizeof(int *));
  
  // assure that all space is allocated continuously:
  arr[0] = (int *)malloc(nrows * ncols * sizeof(int));
  for (i = 1; i < nrows; i++)
    arr[i] = arr[0] + i*ncols;

  return arr;
}


// free a 2D Array that was created using make2DArray
// pre: arr has been cast to (void **)
void free2DArray(void **arr) {
  free(arr[0]);
  free(arr);
}


// set each element of out[0..n-1] to corresponding element of in[0..n-1]
void assignArray(double *out, double *in, int n) {
  int i;

  for (i = 0; i < n; i++)
    out[i] = in[i];
}


// return sum(array[0..length-1])
double sumArray(double *array, int length) {
  double sum = 0;
  int i;

  for (i = 0; i < length; i++)
    sum += array[i];

  return sum;
}


// do an strcmp on s1 and s2, ignoring case
// (convert both to lower case before comparing)
// return value is the same as for strcmp
int strcmpIgnoreCase(const char *s1, const char *s2)  {
  char *s1Lower;
  char *s2Lower;
  int i;
  int result;

  // allocate space for copies
  // note that we need one more than strlen to allow room for termination character
  s1Lower = (char *)malloc((strlen(s1) + 1) * sizeof(char));
  s2Lower = (char *)malloc((strlen(s2) + 1) * sizeof(char));

  for (i = 0; i <= strlen(s1); i++) 
    s1Lower[i] = tolower(s1[i]);
  for (i = 0; i <= strlen(s2); i++)
    s2Lower[i] = tolower(s2[i]);

  result = strcmp(s1Lower, s2Lower);

  free(s1Lower);
  free(s2Lower);

  return result;
}


// If line contains any character in the string commentChars,
//  strip the comment off the line (i.e. replace first occurrence of commentChars with '\0')
// Return 1 if line contains only a comment (or only blanks), 0 otherwise
int stripComment(char *line, const char *commentChars)  {
  char *commentCharLoc;
  int lenTrim; 

  // strip trailing comment:
  commentCharLoc = strpbrk(line, commentChars);
  if (commentCharLoc != NULL)  {
    commentCharLoc[0] = '\0';
  }
  
  // determine length without any leading blanks
  lenTrim = strlen(line) - strspn(line, " \t\n\r");
  return (lenTrim == 0);
}
  

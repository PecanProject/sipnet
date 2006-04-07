/* Basic utility functions

   Author: Bill Sacks

   Creation date: 8/17/04
*/

#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "util.h"

// our own openFile method, which exits gracefully if there's an error
FILE *openFile(char *name, char *mode) {
  FILE *f;

  if ((f = fopen(name, mode)) == NULL) {
    printf("Error opening %s for %s\n", name, mode);
    exit(1);
  }

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
    srandom(t);
  }
  else {
    if (outF != NULL)
      fprintf(outF, "Seeding random number generator with %d\n\n", seed);
    srandom(seed);
  }
}


// returns an exponentially-distributed negative random number 
double randm() {
  double val;

  do {
    val = random()/(RAND_MAX + 1.0);
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


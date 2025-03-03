/* Basic utility functions
    
   Author: Bill Sacks

   Creation date: 8/17/04
*/

#ifndef UTIL_H
#define UTIL_H

#include <stdio.h>

// our own openFile method, which exits gracefully if there's an error
FILE *openFile(char *name, char *mode);

// if seed = 0, seed based on time, otherwise seed based on seed
// if outF is specified (not NULL), write seed to file
void seedRand(unsigned int seed, FILE *outF);

// returns an exponentially-distributed negative random number 
double randm();

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

#endif

/* transpose: A stand-alone program
   Usage: transpose file

   Given a file that contains a rectangular matrix, replace the file with its transpose

   Author: Bill Sacks
   Creation date: 6/5/07
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "util.h"

#define SEPARATORS " \t\n"  // valid separators between items on a line


void usage(char *progName)  {
  printf("Usage: %s filename\n", progName);
  printf("Where filename is the file containing the data to transpose\n");
  printf("This file must contain a rectangular matrix\n");
}


/* Count and return # of lines in f
   Also return character count of the longest line in longestLine
    - includes extra character to hold '\0'
   Assume f is open for reading, and rewound
 */
int numlines(FILE *f, int *longestLine)  {
  int nl;
  char c;
  int length;  // length of this line
  int longLine;

  nl = 0;
  length = 0;
  longLine = 0;
  while ((c = fgetc(f)) != EOF)  {
    length++;
    if (c == '\n')  {
      nl++;
      if (length > longLine)
	longLine = length;
      length = 0;
    }
  }

  *longestLine = longLine + 1;  // add 1 to give space for '\0'
  return nl;
}



/* Read file f into array of lines
   PRE: f is open for reading and rewound
        Each lines[i] is a character array of size maxLength,
	 and lines contains at least nl such character arrays, where nl is the number of lines in the file
   POST: lines[i][j] will contain the jth character on the ith line
         The file pointer f will be at the end of the file
 */
void readFile(FILE *f, char **lines, int maxLength)  {
  int i;

  i=0;
  while(fgets(lines[i], maxLength, f) != NULL)  {  // while not EOF or error
    i++;
  }
}    


/* Count and return number of items in line
    - items are separated by SEPARATORS
 */
int countItems(char *line)  {
  int n;
  char *s;
  char *lineCopy;

  lineCopy = (char *)malloc((strlen(line) + 1) * sizeof(char));
  strcpy(lineCopy, line);

  n = 0;
  s = strtok(lineCopy, SEPARATORS);
  while (s != NULL)  {
    n++;
    s = strtok(NULL, SEPARATORS);
  }

  free(lineCopy);
  return n;
}


/* Read individual items from lines into items
   items[i][j] will hold a pointer to the jth item (j = 0..numItems-1) in the ith line (i = 0..numLines-1)
    - items are separated by SEPARATORS
   There must be exactly numItems items on each line
 */
void readItems(char **lines, char ***items, int numLines, int numItems)  {
  int i, j;

  for (i = 0; i < numLines; i++)  {
    items[i][0] = strtok(lines[i], SEPARATORS);
    for (j = 1; j < numItems; j++)
      items[i][j] = strtok(NULL, SEPARATORS);
    if (items[i][numItems - 1] == NULL)  {
      printf("ERROR: Too few items on line %d\n", i+1);
      exit(1);
    }
    if (strtok(NULL, SEPARATORS) != NULL) {  // more items on the line!
      printf("ERROR: Too many items on line %d\n", i+1);
      exit(1);
    }
  }
}
    

/* Write out the transposed items to f
   f must be open for writing
   items[i][j] holds the jth item (j = 0..numItems-1) on the ith line (i = 0..numLines-1)
   We output all items #0 on the first line, all items #1 on the second line, etc.
   Items are separated by spaces in the output file
 */
void outputTranspose(FILE *f, char ***items, int numLines, int numItems)  {
  int i, j;

  for (j = 0; j < numItems; j++)  {
    for (i = 0; i < numLines; i++)  {
      fprintf(f, "%s ", items[i][j]);
    }
    fprintf(f, "\n");
  }
}


int main(int argc, char *argv[])  {
  FILE *f;
  char *filename;  // name of file to transpose
  char *tmpName;  // name of a temporary file
  int nl;  // number of lines
  int longestLine;  // length of longest line
  int numItems;  // number of items in each line
  char **lines;  // array of character arrays, holding all lines in file
  char ***items;  /* 2-d array of character pointers, holding pointers to various points in the lines array
		     items[i][j] holds a pointer to the jth item in the ith line */
  int i;

  if (argc != 2)  {
    usage(argv[0]);
    exit(1);
  }


  filename = argv[1];
  f = openFile(filename, "r");

  nl = numlines(f, &longestLine);
  rewind(f);

  // Allocate space to hold the file, line by line:
  lines = (char **)malloc(nl * sizeof(char *));
  for (i = 0; i < nl; i++)  {
    lines[i] = (char *)malloc(longestLine * sizeof(char));
  }

  readFile(f, lines, longestLine);
  fclose(f);
  
  // Allocate space to hold the individual items:
  numItems = countItems(lines[0]);
  items = (char ***)malloc(nl * sizeof(char **));
  for (i = 0; i < nl; i++)  {
    items[i] = (char **)malloc(numItems * sizeof(char *));
  }

  readItems(lines, items, nl, numItems);

  // Output to file, replacing existing file:
  tmpName = (char *)malloc((strlen(filename) + 5) * sizeof(char));
  strcpy(tmpName, filename);
  strcat(tmpName, ".bak");
  rename(filename, tmpName);  // keep a backup in case things crash
  f = openFile(filename, "w"); 
  outputTranspose(f, items, nl, numItems);
  fclose(f);
  remove(tmpName);

  // Free space:
  for (i = 0; i < nl; i++)  {
    free(items[i]);
    free(lines[i]);
  }
  free(items);
  free(lines);
  
  return 0;
}

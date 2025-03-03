// Given a binary file with one int (n), then n*l floats,
// write ascii file of floats, n per line

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util.h"

int main(int argc, char *argv[]) {
  FILE *in, *out;
  char inName[128], outName[128];
  int numPerLine;
  float oneFloat;
  int numReadThisLine;
  long numRead;

  if (argc < 2) { 
    printf("Usage: %s fileName\n", argv[0]);
    printf("Where fileName is name of file to decompress\n");
    printf("Decompressed file output in fileName.txt\n");
    exit(1);
  }

  strcpy(inName, argv[1]);
  strcpy(outName, inName);
  strcat(outName, ".txt");

  in = openFile(inName, "r");
  out = openFile(outName, "w");

  fread(&numPerLine, sizeof(int), 1, in); // read numPerLine from in
  
  // loop: read floats until there are no more to read
  numRead = 0;
  numReadThisLine = 0;
  while (fread(&oneFloat, sizeof(float), 1, in) > 0) { // while there's another float
    numRead++;
    numReadThisLine++;
    fprintf(out, "%f ", oneFloat);
    if (numReadThisLine == numPerLine) {
      fprintf(out, "\n"); // time for a new line
      numReadThisLine = 0;
    }
  }

  printf("Read %ld floats\n", numRead);

  fclose(in);
  fclose(out);

  return 0;
}

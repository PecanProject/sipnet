// Given an ascii file of floats, with n floats per line,
// write binary file of floats, prefixed with n
// (i.e. file contains n (integer), then (n*l) floats, where l = # lines)


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// our own openFile method, which exits gracefully if there's an error
FILE *openFile(char *name, char *mode) {
  FILE *f;

  if ((f = fopen(name, mode)) == NULL) {
    printf("Error opening %s for %s\n", name, mode);
    exit(1);
  }

  return f;
}


int main(int argc, char *argv[]) {
  const int MAX_LINE = 1024;
  FILE *in, *out;
  char inName[128], outName[128];
  int numPerLine;
  int i, skipLines;
  long numRead;
  char *status;
  float oneFloat;
  char line[MAX_LINE];

  if (argc < 2) {
    printf("Usage: %s fileName [numLinesToSkip]\n", argv[0]);
    printf("Where fileName is name of file to compress\n");
    printf(" numLinesToSkip is number of initial lines in file to ignore (default: 0)\n");
    printf("Compressed file output in fileName.bin\n");
    exit(1);
  }

  strcpy(inName, argv[1]);
  strcpy(outName, inName);
  strcat(outName, ".bin");

  in = openFile(inName, "r");
  out = openFile(outName, "w");

  if (argc >= 3) // there's a numLinesToSkip argument
    skipLines = strtol(argv[2], &status, 0);
  else
    skipLines = 0;

  // first get number of floats per line:
  // first skip skipLines:
  for (i = 0; i < skipLines; i++)
    fgets(line, MAX_LINE, in); // read & ignore

  // now we're ready to get first real line:
  fgets(line, MAX_LINE, in);
  numPerLine = 0;

  status = strtok(line, " \t");
  while (status != NULL && strcmp(status, "\n") != 0) { // more tokens that aren't "\n"
    numPerLine++;
    status = strtok(NULL, " \t"); // find next token, if it exists
  }
  
  rewind(in); // go back to beginning
  
  fwrite(&numPerLine, sizeof(int), 1, out); // write numPerLine to file

  // skip skipLines:
  for (i = 0; i < skipLines; i++)
    fgets(line, MAX_LINE, in); // read & ignore

  numRead = 0;
  while (fscanf(in, "%f", &oneFloat) > 0) { // while there are more floats
    numRead++;
    fwrite(&oneFloat, sizeof(float), 1, out); // write oneFloat to file
  }

  printf("Read a total of %ld floats\n", numRead);

  fclose(in);
  fclose(out);

  return 0;
}

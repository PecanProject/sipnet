// Bill Sacks
// 11/21/03

// Given an input file <in>, remove any line that doesn't start with
// "199" or "200" (i.e. a year) (allow possible leading whitespace)
// Write result to <in-processed>

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
  FILE *in, *out;
  char inName[64], outName[64];
  char line[512];
  char lineCopy[512];
  char *firstToken;

  // we expect 2 arguments (name of executable & file name)
  if (argc < 2) {
    printf("Usage: %s fileName\n", argv[0]);
    printf("Where fileName is input file, output written to fileName-processed\n");
    exit(1);
  }

  strcpy(inName, argv[1]);
  strcpy(outName, inName);
  strcat(outName, "-processed");

  in = openFile(inName, "r"); // open for reading
  out = openFile(outName, "w");

  // loop: read one line at a time until EOF

  while (fgets(line, sizeof(line), in) != NULL) { // while we haven't reached EOF
    strcpy(lineCopy, line); // because strtok is destructive
    firstToken = strtok(lineCopy, " \t"); // ignore spaces and tabs
    if (strncmp(firstToken, "199", 3) == 0 || strncmp(firstToken, "200", 3) == 0)
      // first three characters are "199" or "200": write to processed file
      fputs(line, out);
      
  }

  fclose(in);
  fclose(out);

  return 0;
}

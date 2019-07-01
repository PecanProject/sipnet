// Bill Sacks
// 10/20/03
// Modified 7/12/06

// Put data in correct format for SiPnET
// Assumes fixed time step lengths

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void usage(char *progName) {
  printf("Usage: %s directory outFileName intervalLength [location], where:\n", progName);
  printf("directory is the directory containing the input climate data\n");
  printf("outFileName is output file for climate data,\n");
  printf("intervalLength is the length of each time step in seconds (e.g. 1/2 hour = 1800 sec.)\n");
  printf("location is location to be written to file, in 1st column\n");
  printf("\t(if no location is specified, use loc. 0)\n");
}

// our own openFile method, which exits gracefully if there's an error
// opens file given by directory/name (or just name if directory is the empty string)
FILE *openFile(char *directory, char *name, char *mode) {
  char fullName[256];
  FILE *f;

  if (strcmp(directory, "") == 0) // no directory specified
    strcpy(fullName, name);
  else {
    strcpy(fullName, directory); 
    strcat(fullName, "/");
    strcat(fullName, name);
  }

  if ((f = fopen(fullName, mode)) == NULL) {
    printf("Error opening %s for %s\n", fullName, mode);
    exit(1);
  }

  return f;
}

int main(int argc, char *argv[]) {
  FILE *tairf, *tsoilf, *parf, *pptf, *vpdf, *vpdSoilf, *vpressf, *wspdf, *soilwetnessf, *out;

  /* Harvard Forest:
  int day = 1;
  int year = 1992;
  */

  int day = 305;
  int year = 1998;

  int time = 0; // time of start of interval (in seconds)
  int leap = year%4; // for leap year calculations

  // climate variables:
  float tair, tsoil, par, precip, vpd, vpdSoil, vpress, wspd, soilwetness;

  char *directory, *outFileName; // pointers into the argv vector
  int intervalLength, location;
  char *errc;

  if (argc < 4 || argc > 5) {
    usage(argv[0]);
    exit(1);
  }

  // read arguments
  directory = argv[1];
  outFileName = argv[2];
  intervalLength = strtol(argv[3], &errc, 0);
  if (argc == 5) // a location was specified
    location = strtol(argv[4], &errc, 0);
  else
    location = 0;
  // done reading arguments

  tairf = openFile(directory, "tairstep", "r");
  tsoilf = openFile(directory, "tsoilstep", "r");
  parf = openFile(directory, "parstep", "r");
  pptf = openFile(directory, "pptstep", "r");
  vpdf = openFile(directory, "vpdstep", "r");
  vpdSoilf = openFile(directory, "vpdsoilstep", "r");
  vpressf = openFile(directory, "vpressstep", "r");
  wspdf = openFile(directory, "wspdstep", "r");
  soilwetnessf = openFile(directory, "soilwetnessstep", "r");
  out = openFile("", outFileName, "w");

  while (fscanf(tairf, "%f", &tair) > 0) { // while we haven't reached end of file - assume all files have the same length
    fscanf(tsoilf, "%f", &tsoil);
    fscanf(parf, "%f", &par);
    fscanf(pptf, "%f", &precip);
    fscanf(vpdf, "%f", &vpd);
    fscanf(vpdSoilf, "%f", &vpdSoil);
    fscanf(vpressf, "%f", &vpress);
    fscanf(wspdf, "%f", &wspd);
    fscanf(soilwetnessf, "%f", &soilwetness);

    fprintf(out, "%d\t%4d %3d %5.2f %8d %8.4f %8.4f %8.4f %8.4f %8.4f %8.4f %8.4f %8.4f %8.4f\n", location, year, day, time/3600., -intervalLength, tair, tsoil, par, precip, vpd, vpdSoil, vpress, wspd, soilwetness);  // note that we output time in hours rather than seconds, and note that we output intervalLength as negative to indicate that it is in seconds
    
    time = time + intervalLength;
    while (time >= 86400) { // next day
      time = time - 86400;
      if ((day == 365 && leap > 0) || (day == 366 && leap == 0)) {
	day = 1;
	year++;
	leap = year%4;
      }
      else
	day++;
    }
  }

  fclose(tairf);
  fclose(tsoilf);
  fclose(parf);
  fclose(pptf);
  fclose(vpdf);
  fclose(vpdSoilf);
  fclose(vpressf);
  fclose(wspdf);
  fclose(soilwetnessf);
  fclose(out);

  return 0;
}

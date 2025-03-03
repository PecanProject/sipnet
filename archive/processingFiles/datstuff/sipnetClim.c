// Bill Sacks
// 6/28/02

// Put data in correct format for SiPnET

#include <stdio.h>
#include <stdlib.h>

void usage(char *progName) {
  printf("Usage: %s fileName [location], where:\n", progName);
  printf("fileName is output file for climate data,\n");
  printf("location is location to be written to file, in 1st column\n");
  printf("\t(if no location specified, use loc. 0)\n");
}

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
  FILE *intervalLengthf, *tairf, *tsoilf, *parf, *waterInf, *vpdf, *vpdSoilf, *vpressf, *wspdf, *soilwetnessf, *out;

  /* Harvard Forest:
  int day = 1;
  int year = 1992;
  */

  int day = 305;
  int year = 1998;
  float time = 0.0; // time of start of interval (in hours)
  int leap = year%4; // for leap year calculations
  float intervalLength; // in hours

  // climate variables:
  float tair, tsoil, par, waterIn, vpd, vpdSoil, vpress, wspd, soilwetness;

  int location;
  char *errc;

  if (argc < 2 || argc > 3) {
    usage(argv[0]);
    exit(1);
  }

  intervalLengthf = openFile("niwot/climate/intervaldn", "r");
  tairf = openFile("niwot/climate/tairdn", "r");
  tsoilf = openFile("niwot/climate/tsoildn", "r");
  parf = openFile("niwot/climate/pardn", "r");
  waterInf = openFile("niwot/climate/pptdn", "r"); // waterIn = precip.
  vpdf = openFile("niwot/climate/vpddn", "r");
  vpdSoilf = openFile("niwot/climate/vpdsoildn", "r");
  vpressf = openFile("niwot/climate/vpressdn", "r");
  wspdf = openFile("niwot/climate/wspddn", "r");
  soilwetnessf = openFile("niwot/climate/soilwetnessdn", "r");
  out = openFile(argv[1], "w");

  if (argc == 3) // a location was specified
    location = strtol(argv[2], &errc, 0);
  else // use default location: 0
    location = 0;

  while (fscanf(intervalLengthf, "%f", &intervalLength) > 0) { // while we haven't reached end of file
    intervalLength /= 24.0; // convert from hours to days
    fscanf(tairf, "%f", &tair);
    fscanf(tsoilf, "%f", &tsoil);
    fscanf(parf, "%f", &par);
    fscanf(waterInf, "%f", &waterIn);
    fscanf(vpdf, "%f", &vpd);
    fscanf(vpdSoilf, "%f", &vpdSoil);
    fscanf(vpressf, "%f", &vpress);
    fscanf(wspdf, "%f", &wspd);
    fscanf(soilwetnessf, "%f", &soilwetness);
    
    fprintf(out, "%d\t%4d %3d %5.2f %4.3f %8.4f %8.4f %8.4f %8.4f %8.4f %8.4f %8.4f %8.4f %8.4f\n", location, year, day, time, intervalLength, tair, tsoil, par, waterIn, vpd, vpdSoil, vpress, wspd, soilwetness);
    
    time = time + intervalLength*24.0;
    while (time >= 24.0) { // next day
      time = time - 24.0;
      if ((day == 365 && leap > 0) || (day == 366 && leap == 0)) {
	day = 1;
	year++;
	leap = year%4;
      }
      else
	day++;
    }
  }
  
  fclose(intervalLengthf);
  fclose(tairf);
  fclose(tsoilf);
  fclose(parf);
  fclose(waterInf);
  fclose(vpdf);
  fclose(vpdSoilf);
  fclose(vpressf);
  fclose(wspdf);
  fclose(soilwetnessf);
  fclose(out);

  return 0;
}

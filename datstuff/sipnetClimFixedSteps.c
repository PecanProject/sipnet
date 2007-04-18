// Bill Sacks
// 10/20/03

// Put data in correct format for SiPnET
// Assumes fixed time step lengths

#include <stdio.h>
#include <stdlib.h>

int main() {
  const float INTERVAL_LENGTH = 3.0/24.0; // in days

  FILE *tairf, *tsoilf, *parf, *waterInf, *vpdf, *out;
  int day = 1;
  int year = 1992;
  float time = 0.0; // time of start of interval (in hours)
  int leap = year%4; // for leap year calculations

  // climate variables:
  float tair, tsoil, par, waterIn, vpd;


  if ((tairf = fopen("data/tairstep", "r")) == NULL) {
    printf("Can't open tair\n");
    exit(1);
  }

  if ((tsoilf = fopen("data/tsoilstep", "r")) == NULL) {
    printf("Can't open tsoil\n");
    exit(1);
  }

  if ((parf = fopen("data/parstep", "r")) == NULL) {
    printf("Can't open parday\n");
    exit(1);
  }

  // for now, use precip. as estimate of waterIn
  // WE'LL WANT TO CHANGE THIS!
  if ((waterInf = fopen("data/pptstep", "r")) == NULL) {
    printf("Can't open pptday\n");
    exit(1);
  }

  if ((vpdf = fopen("data/vpdstep", "r")) == NULL) {
    printf("Can't open vpdday\n");
    exit(1);
  }

  if ((out = fopen("harv.clim", "w")) == NULL) {
    printf("Can't open harv.clim\n");
    exit(1);
  }

  while (year < 2002) { // while we haven't reached end of file
    fscanf(tairf, "%f", &tair);
    fscanf(tsoilf, "%f", &tsoil);
    fscanf(parf, "%f", &par);
    fscanf(waterInf, "%f", &waterIn);
    fscanf(vpdf, "%f", &vpd);
    
    fprintf(out, "%4d %3d %5.2f %4.3f %8.4f %8.4f %8.4f %8.4f %8.4f\n", year, day, time, INTERVAL_LENGTH, tair, tsoil, par, waterIn, vpd);
    
    time = time + INTERVAL_LENGTH*24.0;
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

  fclose(tairf);
  fclose(tsoilf);
  fclose(parf);
  fclose(waterInf);
  fclose(vpdf);
  fclose(out);

  return 0;
}

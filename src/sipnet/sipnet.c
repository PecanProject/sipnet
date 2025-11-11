/* SiPnET: Simplified PnET

   Author: Bill Sacks  wsacks@wisc.edu
   modified by...
   John Zobitz zobitz@augsburg.edu
   Dave Moore dmoore1@ucar.edu

   A simple box model of carbon cycling
   largely based on PnET
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#include "common/context.h"
#include "common/exitCodes.h"
#include "common/logging.h"
#include "common/util.h"

#include "sipnet.h"
#include "events.h"
#include "outputItems.h"
#include "runmean.h"
#include "state.h"

#define C_WEIGHT 12.0  // molecular weight of carbon
// #define TEN_6 1000000.0  // for conversions from micro
#define TEN_9 1000000000.0  // for conversions from nano
#define SEC_PER_DAY 86400.0

// constants for tracking running mean of NPP:
#define MEAN_NPP_DAYS 5  // over how many days do we keep the running mean?
#define MEAN_NPP_MAX_ENTRIES (MEAN_NPP_DAYS * 50)  //
// assume that the most pts we can have is two per hour

// constants for tracking running mean of GPP:
#define MEAN_GPP_SOIL_DAYS 5  // over how many days do we keep the running mean?
#define MEAN_GPP_SOIL_MAX_ENTRIES (MEAN_GPP_SOIL_DAYS * 50)  //
// assume that the most pts we can have is one per hour

// some constants for water submodel:
#define LAMBDA 2501000.  // latent heat of vaporization (J/kg)
#define LAMBDA_S 2835000.  // latent heat of sublimation (J/kg)
#define RHO 1.3  // air density (kg/m^3)
#define CP 1005.  // specific heat of air (J/(kg K))
#define GAMMA 66.  // psychometric constant (Pa/K)
#define E_STAR_SNOW 0.6  //
/* approximate saturation vapor pressure at 0 degrees C (kPa)
   (we assume snow temperature is 0 degrees C or slightly lower) */

#define TINY 0.000001  // to avoid those nasty divide-by-zero errors

// end constant definitions

// Sections of code below are labeled as to the source paper and content
// (usually equation) that they represent. The following sources are used:
//
// [1] Braswell, Bobby H., William J. Sacks, Ernst Linder, and David S.
//     Schimel. 2005. Estimating Diurnal to Annual Ecosystem Parameters by
//     Synthesis of a Carbon Flux Model with Eddy Covariance Net Ecosystem
//     Exchange Observations. Global Change Biology 11 (2): 335–55.
//     https://doi.org/10.1111/j.1365-2486.2005.00897.x
// Braswell at al.'s original description of SIPNET
//
// [2] Sacks WJ, Schimel DS, Monson RK, Braswell BH. 2006. Model-data synthesis
//     of diurnal and seasonal CO2 fluxes at Niwot Ridge, Colorado. Global
//     Change Biology 12:240–259
//     https://doi.org/10.1111/j.1365-2486.2005.01059.x
// Sacks, et al. additions of evergreen leaf phenology, and more
// complex soil moisture model including a snowpack and litter layer
// (referred to as 'surface layer' in this publication)
//
// [3] Zobitz, J. M., D. J. P. Moore, W. J. Sacks, R. K. Monson, D. R.
//     Bowling, and D. S. Schimel. 2008. “Integration of Process-Based Soil
//     Respiration Models with Whole-Ecosystem CO2 Measurements.” Ecosystems
//     11 (2): 250–69. https://doi.org/10.1007/s10021-007-9120-1
// Zobitz, et al. additions of roots (also, soil multi-pool and soil
// quality models, which are not currently included in this SIPNET - but we
// now know the source if/when we want to reintroduce them)
//
// [4] Zobitz, J. M., et al(?)/publication unknown, currently
//     '/docs/soilSIPNET_draft.pdf' in the repo and appears to be Chapter 5 of
//     an unknown book
// Zobitz et al.: this appears to be a prior draft of [3] above, which
// includes the addition of microbes
//
// [5] LeBauer et al. (unpublished) "SIPNET 2: A lightweight, extensible model
//     for coupled C–N–H₂O–GHG dynamics in managed ecosystems"
// LeBauer et al.: addition of events and nitrogen cycle
//
// Note that Sacks, et al. (2007) is not referenced directly here, but is of
// interest in that it represents a use of the more complex soil moisture
// model from [2] above _without_ the litter layer.
// Other cites/notes:
// * [3] and [4] above are sources for the now-removed multi-pool/soil quality
//   modes, if we want to bring those back at some point
// * Zobitz, J.M., Moore, D.J.P., Quaife, T., Braswell, B.H., Bergeson, A.,
//   Anthony, J.A., Monson, R.K., 2014. Joint data assimilation of satellite
//   reflectance and net ecosystem exchange data constrains ecosystem carbon
//   fluxes at a high-elevation subalpine forest. Agric. For. Meteorol.
//   195–196, 73–88. https://doi.org/10.1016/j.agrformet.2014.04.011
//   - This paper details the now-removed MODIS and fAPAR tracking, if we want
//   to bring those back at some point

// more global variables:
// these are global to increase efficiency (avoid lots of parameter passing)

// running mean of NPP over some fixed time
// (stored in g C * m^-2 * day^-1)
static MeanTracker *meanNPP;
// running mean of GPP over some fixed time for linkages
// (stored in g C * m^-2 * day^-1)
static MeanTracker *meanGPP;

//
// Infrastructure and I/O functions
//

/*!
 * Read climate file into linked list
 *
 * Each line of the climate file represents one time step, with the following
 * format:
 *    year day time intervalLength tair tsoil par precip vpd vpdSoil vPress wspd
 soilWetness
 *
 * NOTE: there should be NO blank lines in the file.

 * @param climFile Name of climate file
 */
void readClimData(const char *climFile) {
  FILE *in;
  ClimateNode *curr, *next;
  int year, day;
  int lastYear = -1;
  double time, length;  // time in hours, length in days (or fraction of day)
  double tair, tsoil, par, precip, vpd, vpdSoil, vPress, wspd, soilWetness;

  double thisGdd;  // growing degree days of this time step
  double gdd = 0.0;  // growing degree days since the last Jan. 1

  int status;  // status of the read

  // for format check
  int firstLoc, dummyLoc, numFields;
  int expectedNumCols;
  int legacyFormat;
  char *firstLine = NULL;
  size_t lineCap = 0;
  const char *SEPARATORS = " \t\n\r";  // characters that can separate values in
                                       // parameter files

  in = openFile(climFile, "r");

  // Check format of first line to see if location is still specified (we will
  // ignore it if so)
  if (getline(&firstLine, &lineCap, in) == -1) {  // EOF
    logError("no climate data in %s\n", climFile);
    exit(EXIT_CODE_INPUT_FILE_ERROR);
  }

  numFields = countFields(firstLine, SEPARATORS);
  switch (numFields) {
    case NUM_CLIM_FILE_COLS:
      // Standard format
      expectedNumCols = NUM_CLIM_FILE_COLS;
      legacyFormat = 0;
      break;
    case NUM_CLIM_FILE_COLS_LEGACY:
      expectedNumCols = NUM_CLIM_FILE_COLS_LEGACY;
      legacyFormat = 1;
      logWarning("old climate file format detected (found %d cols); ignoring "
                 "location and soilWetness columns in %s\n",
                 numFields, climFile);
      break;
    default:
      // Unrecognized format
      logError("format unrecognized in climate file %s; %d columns found, "
               "expected %d or %d (legacy format)\n",
               climFile, numFields, NUM_CLIM_FILE_COLS,
               NUM_CLIM_FILE_COLS_LEGACY);
      exit(EXIT_CODE_INPUT_FILE_ERROR);
  }

  if (legacyFormat) {
    status = sscanf(firstLine,  // NOLINT
                    "%d %d %d %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf",
                    &firstLoc, &year, &day, &time, &length, &tair, &tsoil, &par,
                    &precip, &vpd, &vpdSoil, &vPress, &wspd, &soilWetness);
  } else {
    status = sscanf(firstLine,  // NOLINT
                    "%d %d %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf", &year,
                    &day, &time, &length, &tair, &tsoil, &par, &precip, &vpd,
                    &vpdSoil, &vPress, &wspd);
  }
  free(firstLine);

  if (status != expectedNumCols) {
    logError("while reading climate file: bad data on first line\n");
    exit(EXIT_CODE_INPUT_FILE_ERROR);
  }

  firstClimate = (ClimateNode *)malloc(sizeof(ClimateNode));
  next = firstClimate;

  while (status != EOF) {
    // we have another day's climate
    curr = next;

    curr->year = year;
    curr->day = day;
    curr->time = time;

    if (length < 0) {  // parse as seconds
      length = length / -86400.;  // convert to days
    }
    curr->length = length;

    curr->tair = tair;
    curr->tsoil = tsoil;
    curr->par = par * (1.0 / length);
    // convert par from Einsteins * m^-2 to Einsteins * m^-2 * day^-1
    curr->precip = precip * 0.1;  // convert from mm to cm
    curr->vpd = vpd * 0.001;  // convert from Pa to kPa
    if (curr->vpd < TINY) {
      curr->vpd = TINY;  // avoid divide by zero
    }
    curr->vpdSoil = vpdSoil * 0.001;  // convert from Pa to kPa
    curr->vPress = vPress * 0.001;  // convert from Pa to kPa
    curr->wspd = wspd;
    if (curr->wspd < TINY) {
      curr->wspd = TINY;  // avoid divide by zero
    }

    if (ctx.gdd) {
      // :: from [1], growing degree day calculations to support described
      //    modification to leaf phenology on pg 350
      if (year != lastYear) {  // HAPPY NEW YEAR!
        gdd = 0;  // reset growing degree days
      }
      thisGdd = tair * length;
      if (thisGdd < 0) {  // can't have negative growing degree days
        thisGdd = 0;
      }
      gdd += thisGdd;
      curr->gdd = gdd;
    }

    lastYear = year;

    if (legacyFormat) {
      status =
          fscanf(in,  // NOLINT
                 "%d %d %d %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf",
                 &dummyLoc, &year, &day, &time, &length, &tair, &tsoil, &par,
                 &precip, &vpd, &vpdSoil, &vPress, &wspd, &soilWetness);
    } else {
      status = fscanf(in,  // NOLINT
                      "%d %d %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf", &year,
                      &day, &time, &length, &tair, &tsoil, &par, &precip, &vpd,
                      &vpdSoil, &vPress, &wspd);
    }
    if (status != EOF) {
      if (status != expectedNumCols) {
        logError("while reading climate file: bad data near year %d day %d\n",
                 year, day);
        exit(EXIT_CODE_INPUT_FILE_ERROR);
      }
      // Check for older file with multiple locations - that's an error now
      if (legacyFormat && (dummyLoc != firstLoc)) {
        logError("while reading legacy climate file %s: multiple locations "
                 "not supported (locations found: %d and %d)\n",
                 climFile, firstLoc, dummyLoc);
        exit(EXIT_CODE_INPUT_FILE_ERROR);
      }

      // add a new climate node at end of linked list
      next = (ClimateNode *)malloc(sizeof(ClimateNode));
      curr->nextClim = next;
      // set this down here rather than at top of loop so head treated the
      // same as rest of list
    } else {  // status == EOF - no more records
      curr->nextClim = NULL;  // terminate this last linked list
    }
  }  // end while

  fclose(in);
}

/*!
 * Read initial model parameter values from param file
 *
 * Note that the last argument in the "initializeOneModelParam" function
 * indicates whether the given parameter is required:
 *   1 -> must be in param file
 *   0 -> optional
 *
 * @param modelParamsPtr ModelParams struct, will be alloc'd here
 * @param paramFile Name of parameter file
 */
void readParamData(ModelParams **modelParamsPtr, const char *paramFile) {
  FILE *paramF;
  ModelParams *modelParams;
  paramF = openFile(paramFile, "r");

  *modelParamsPtr = newModelParams(NUM_PARAMS);
  modelParams = *modelParamsPtr;  // to prevent lots of unnecessary dereferences

  // clang-format off
  // NOLINTBEGIN
  initializeOneModelParam(modelParams, "plantWoodInit", &(params.plantWoodInit), 1);
  initializeOneModelParam(modelParams, "laiInit", &(params.laiInit), 1);
  initializeOneModelParam(modelParams, "litterInit", &(params.litterInit), 1);
  initializeOneModelParam(modelParams, "soilInit", &(params.soilInit), 1);
  initializeOneModelParam(modelParams, "soilWFracInit", &(params.soilWFracInit), 1);
  initializeOneModelParam(modelParams, "snowInit", &(params.snowInit), 1);
  initializeOneModelParam(modelParams, "aMax", &(params.aMax), 1);
  initializeOneModelParam(modelParams, "aMaxFrac", &(params.aMaxFrac), 1);
  initializeOneModelParam(modelParams, "baseFolRespFrac", &(params.baseFolRespFrac), 1);

  initializeOneModelParam(modelParams, "psnTMin", &(params.psnTMin), 1);
  initializeOneModelParam(modelParams, "psnTOpt", &(params.psnTOpt), 1);
  initializeOneModelParam(modelParams, "vegRespQ10", &(params.vegRespQ10), 1);
  initializeOneModelParam(modelParams, "growthRespFrac", &(params.growthRespFrac), ctx.growthResp);
  initializeOneModelParam(modelParams, "frozenSoilFolREff", &(params.frozenSoilFolREff), 1);
  initializeOneModelParam(modelParams, "frozenSoilThreshold", &(params.frozenSoilThreshold), 1);
  initializeOneModelParam(modelParams, "dVpdSlope", &(params.dVpdSlope), 1);
  initializeOneModelParam(modelParams, "dVpdExp", &(params.dVpdExp), 1);
  initializeOneModelParam(modelParams, "halfSatPar", &(params.halfSatPar), 1);
  initializeOneModelParam(modelParams, "attenuation", &(params.attenuation), 1);

  initializeOneModelParam(modelParams, "leafOnDay", &(params.leafOnDay), !((ctx.gdd) || (ctx.soilPhenol)));
  initializeOneModelParam(modelParams, "gddLeafOn", &(params.gddLeafOn), ctx.gdd);
  initializeOneModelParam(modelParams, "soilTempLeafOn", &(params.soilTempLeafOn), ctx.soilPhenol);
  initializeOneModelParam(modelParams, "leafOffDay", &(params.leafOffDay), 1);
  initializeOneModelParam(modelParams, "leafGrowth", &(params.leafGrowth), 1);
  initializeOneModelParam(modelParams, "fracLeafFall", &(params.fracLeafFall), 1);
  initializeOneModelParam(modelParams, "leafAllocation", &(params.leafAllocation), 1);
  initializeOneModelParam(modelParams, "leafTurnoverRate", &(params.leafTurnoverRate), 1);
  initializeOneModelParam(modelParams, "baseVegResp", &(params.baseVegResp), 1);
  initializeOneModelParam(modelParams, "litterBreakdownRate", &(params.litterBreakdownRate), ctx.litterPool);

  initializeOneModelParam(modelParams, "fracLitterRespired", &(params.fracLitterRespired), ctx.litterPool);
  initializeOneModelParam(modelParams, "baseSoilResp", &(params.baseSoilResp), 1);
  initializeOneModelParam(modelParams, "soilRespQ10", &(params.soilRespQ10), 1);

  initializeOneModelParam(modelParams, "soilRespMoistEffect", &(params.soilRespMoistEffect), ctx.waterHResp);
  initializeOneModelParam(modelParams, "waterRemoveFrac", &(params.waterRemoveFrac), 1);
  initializeOneModelParam(modelParams, "frozenSoilEff", &(params.frozenSoilEff), 1);
  initializeOneModelParam(modelParams, "wueConst", &(params.wueConst), 1);
  initializeOneModelParam(modelParams, "soilWHC", &(params.soilWHC), 1);
  initializeOneModelParam(modelParams, "immedEvapFrac", &(params.immedEvapFrac), 1);
  initializeOneModelParam(modelParams, "fastFlowFrac", &(params.fastFlowFrac), 1);
  initializeOneModelParam(modelParams, "leafPoolDepth", &(params.leafPoolDepth), ctx.leafWater);

  initializeOneModelParam(modelParams, "snowMelt", &(params.snowMelt), ctx.snow);
  initializeOneModelParam(modelParams, "rdConst", &(params.rdConst), 1);
  initializeOneModelParam(modelParams, "rSoilConst1", &(params.rSoilConst1), 1);
  initializeOneModelParam(modelParams, "rSoilConst2", &(params.rSoilConst2), 1);
  initializeOneModelParam(modelParams, "leafCSpWt", &(params.leafCSpWt), 1);
  initializeOneModelParam(modelParams, "cFracLeaf", &(params.cFracLeaf), 1);
  initializeOneModelParam(modelParams, "woodTurnoverRate", &(params.woodTurnoverRate), 1);

  initializeOneModelParam(modelParams, "efficiency", &(params.efficiency), ctx.microbes);
  initializeOneModelParam(modelParams, "maxIngestionRate", &(params.maxIngestionRate), ctx.microbes);
  initializeOneModelParam(modelParams, "halfSatIngestion", &(params.halfSatIngestion), ctx.microbes);
  initializeOneModelParam(modelParams, "microbeInit", &(params.microbeInit), ctx.microbes);
  initializeOneModelParam(modelParams, "fineRootFrac", &(params.fineRootFrac), 1);
  initializeOneModelParam(modelParams, "coarseRootFrac", &(params.coarseRootFrac), 1);

  initializeOneModelParam(modelParams, "fineRootAllocation", &(params.fineRootAllocation), 1);
  initializeOneModelParam(modelParams, "woodAllocation", &(params.woodAllocation), 1);

  initializeOneModelParam(modelParams, "fineRootExudation", &(params.fineRootExudation), 1);
  initializeOneModelParam(modelParams, "coarseRootExudation", &(params.coarseRootExudation), 1);
  initializeOneModelParam(modelParams, "fineRootTurnoverRate", &(params.fineRootTurnoverRate), 1);
  initializeOneModelParam(modelParams, "coarseRootTurnoverRate", &(params.coarseRootTurnoverRate), 1);
  initializeOneModelParam(modelParams, "baseFineRootResp", &(params.baseFineRootResp), 1);
  initializeOneModelParam(modelParams, "baseCoarseRootResp", &(params.baseCoarseRootResp), 1);
  initializeOneModelParam(modelParams, "fineRootQ10", &(params.fineRootQ10), 1);
  initializeOneModelParam(modelParams, "coarseRootQ10", &(params.coarseRootQ10), 1);

  initializeOneModelParam(modelParams, "baseMicrobeResp", &(params.baseMicrobeResp), ctx.microbes);
  initializeOneModelParam(modelParams, "microbeQ10", &(params.microbeQ10), ctx.microbes);
  initializeOneModelParam(modelParams, "microbePulseEff", &(params.microbePulseEff), ctx.microbes);

  // Nitrogen cycle params from [5] LeBauer et al. (unpublished)
  initializeOneModelParam(modelParams, "mineralNInit", &(params.minNInit), ctx.nitrogenCycle);
  initializeOneModelParam(modelParams, "nVolatilization", &(params.nVolatilization), ctx.nitrogenCycle);
  initializeOneModelParam(modelParams, "nLeachingFrac", &(params.nLeachingFrac), ctx.nitrogenCycle);

  // NOLINTEND
  // clang-format on

  readModelParams(modelParams, paramF);

  fclose(paramF);
}

/*!
 * Print header row to output file
 *
 * @param out File pointer for output
 */
void outputHeader(FILE *out) {
  fprintf(out, "Notes: (PlantWoodC, PlantLeafC, Soil and Litter in g C/m^2; "
               "Water and Snow in cm; SoilWetness is fraction of WHC;\n");
  fprintf(out, "year day time plantWoodC plantLeafC woodCreation ");
  fprintf(out, "soil microbeC coarseRootC fineRootC ");
  fprintf(out, "litter soilWater soilWetnessFrac snow ");
  fprintf(out,
          "npp nee cumNEE gpp rAboveground rSoil rRoot ra rh rtot "
          "evapotranspiration fluxestranspiration minN n2oFlux nLeachFlux\n");
}

/*!
 * Print current state values to output file
 * @param out File pointer for output
 * @param year
 * @param day
 * @param time
 */
void outputState(FILE *out, int year, int day, double time) {

  fprintf(out, "%4d %3d %5.2f %8.2f %8.2f %8.2f ", year, day, time,
          envi.plantWoodC, envi.plantLeafC, trackers.woodCreation);
  fprintf(out, "%8.2f ", envi.soil);
  fprintf(out, "%8.2f ", envi.microbeC);
  fprintf(out, "%8.2f %8.2f", envi.coarseRootC, envi.fineRootC);
  fprintf(out, " %8.2f %8.2f %8.3f %8.2f ", envi.litter, envi.soilWater,
          trackers.soilWetnessFrac, envi.snow);
  fprintf(out,
          "%8.2f %8.2f %8.2f %8.2f %8.3f %8.3f %8.3f %8.3f %8.3f %8.3f %8.8f "
          "%8.4f %8.3f %8.6f %8.4f\n",
          trackers.npp, trackers.nee, trackers.totNee, trackers.gpp,
          trackers.rAboveground, trackers.rSoil, trackers.rRoot, trackers.ra,
          trackers.rh, trackers.rtot, trackers.evapotranspiration,
          fluxes.transpiration, envi.minN, fluxes.nVolatilization,
          fluxes.nLeaching);
}

// de-allocate space used for climate linked list
void freeClimateList() {
  ClimateNode *curr, *prev;

  curr = firstClimate;
  while (curr != NULL) {
    prev = curr;
    curr = curr->nextClim;
    free(prev);
  }
}

//
// Modeling functions
//

/*!
 * @brief Compute canopy light effect using Simpson's rule.
 *
 * Similar to light attenuation in PnET, first calculate light
 * intensity and then the light effect `lightEFF` for each layer.
 *
 * Integrating Light Effect over the canopy, from top to bottom, approximated
 * numerically using Simpson's method. Simpson's rule requires an odd number of
 * points, thus NUM_LAYERS must be EVEN because we loop from layer = 0 to layer
 * = NUM_LAYERS
 *
 * Simpson's rule approximates the integral as:
 *   (h/3) * (y(0) + 4y(1) + 2y(2) + 4y(3) + ... + 2y(n-2) + 4y(n-1) + y(n)),
 *   where h is the distance between each x value (here h = 1/NUM_LAYERS).
 *   We keep track of the running sum in cumLightEff.
 *
 * cumLai is 0 at the canopy top and equals total LAI at the bottom.
 *
 * Note: SIPNET is not a multi-layer model; this function derives a
 * canopy‐averaged light effect that is then used to calculate calculate GPP for
 * the whole canopy.
 *
 * @param[out] lightEff Canopy average light effect.
 * @param[in] lai Leaf area index (m^2 leaf/m^2 ground).
 * @param[in] par Incoming Photosynthetically Active Radiation (PAR).
 */
void calcLightEff(double *lightEff, double lai, double par) {

  // Information on the distribution of LAI with height is available
  // as of March 2007 ... contact Dr. Maggie Prater Maggie.Prater@colorado.edu

  // All of this modeling comes from [1]

  static const int NUM_LAYERS = 6;
  // believe it or not, 6 layers gives approximately the same result as 100
  // layers

  int layer;  // counter
  double cumLai;  // lai from this layer up
  double lightIntensity;
  double currLightEff = 0.0, cumLightEff;
  int coeff;  // current coefficient in Simpson's rule

  if (lai > 0 && par > 0) {  // must have at least some leaves and some light
    cumLightEff = 0.0;  // the running sum
    layer = 0;
    coeff = 1;

    while (layer <= NUM_LAYERS) {  // layer 0 means top layer
      // lai from this layer up, starting at top
      // :: from [1], description above eq (A11)
      cumLai = lai * ((double)layer / NUM_LAYERS);

      // par attenuated down to current layer
      // :: from [1], eq (A11)
      lightIntensity = par * exp(-1.0 * params.attenuation * cumLai);

      // between 0 and 1; when lightIntensity = halfSatPar, currLightEff = 1/2
      // :: from [1], eq (A12), modified as power of 2 instead of e (but
      // equivalent mathematically)
      currLightEff = (1 - pow(2, (-1.0 * lightIntensity / params.halfSatPar)));

      // CHANGE FROM [1]: instead of taking mean of the light effects at each
      // layer, we approximate the integral via simpson's rule, and then divide
      // by the number of layers
      cumLightEff += coeff * currLightEff;

      // now move to the next layer:
      layer++;
      coeff = 2 * (1 + layer % 2);  // coeff. goes 1, 4, 2, 4, ..., 2, 4, 2
    }
    // last value should have had a coefficient of 1, but actually had a
    // coefficient of 2, so subtract 1:
    cumLightEff -= currLightEff;
    *lightEff = cumLightEff / (3.0 * NUM_LAYERS);  // multiplying by (h/3) in
                                                   // Simpson's rule
  } else {  // no leaves or no light!
    *lightEff = 0;
  }
}

/*!
 * @brief Compute gross potential photosynthesis with restrictions and base
 * foliar resp
 *
 * Computes GPP_pot from [1], eq (A7); GPP reduced by effects of temperature,
 * vapor pressure deficit, and light. Also computes base foliar respiration here
 * for convenience, used later as part of foliar respiration computation.
 *
 * @param[out] potGrossPsn gross photosynthesis without water effect
 *                         (g C * m^-2 ground area * day^-1)
 * @param[out] baseFolResp base foliar respiration unmodified by temp, water,
 *                         etc. (g C * m^-2 ground area * day^-1)
 * @param[in] lai leaf area index (m^2 leaf * m^-2 ground area)
 * @param[in] tair air temperature (degrees Celsius)
 * @param[in] vpd vapor pressure deficit (kPa)
 * @param[in] par photosynthetically active radiation (Einsteins * m^-2 ground
 *                area * day^-1)
 */
void potPsn(double *potGrossPsn, double *baseFolResp, double lai, double tair,
            double vpd, double par) {
  // Calculation of potGrossPsn proceeds as described in [1], with minor
  // modifications as noted below.

  // Calculation of baseFolResp is used as part of [1], eq (A18), calculated
  // here for convenience (see vegResp() fur use of baseFolResp).

  // maximum possible gross respiration (nmol CO2 * g^-1 leaf * sec^-1)
  double grossAMax;
  // effect of temperature on photosynthesis (range: [0:1])
  double dTemp;
  // decrease in leaf gas exchange due to vapor pressure deficit
  double dVpd;
  // decrease in photosynthesis due to amt. of light absorbed
  double dLight;
  // base foliar respiration in nmol CO2 * g^-1 leaf * sec^-1
  double respPerGram;
  // convert from (nmol CO2 * g^-1 leaf * sec^-1) to
  // (g C * m^-2 ground area * day^-1)
  double conversion;

  // foliar respiration, unmodified by temp, etc.
  // :: from [1], eq (A5)
  respPerGram = params.baseFolRespFrac * params.aMax;
  // daily maximum gross photosynthetic rate
  // :: from [1], eq (A6)
  grossAMax = params.aMax * params.aMaxFrac + respPerGram;

  // Now to calculate reductions to the daily maximum - dTemp, dVpd, dLight
  // :: from [1], eq (A9)
  dTemp = (params.psnTMax - tair) * (tair - params.psnTMin) /
          pow((params.psnTMax - params.psnTMin) / 2.0, 2);
  dTemp = fmax(dTemp, 0.0);
  // :: from [1], eq (A10); modified to accept a variable exponent, [1] uses
  // dVpdExp = 2 [TAG:UNKNOWN_PROVENANCE] vpd exponent
  dVpd = 1.0 - params.dVpdSlope * pow(vpd, params.dVpdExp);
  dVpd = fmax(dVpd, 0.0);
  // dLight calculated as described in [1], see calcLightEff()
  calcLightEff(&dLight, lai, par);

  // :: from [1], unit conversion taking into account eq (A8)
  conversion = C_WEIGHT * (1.0 / TEN_9) *
               (params.leafCSpWt / params.cFracLeaf) * lai *
               SEC_PER_DAY;  // to convert units
  // :: from [1], eq (A7)
  *potGrossPsn = grossAMax * dTemp * dVpd * dLight * conversion;

  // do foliar resp. even if no photosynthesis in this time step
  // :: from [1] in part, used in eq (A18) later
  *baseFolResp = respPerGram * conversion;
}

// calculate transpiration (cm H20 * day^-1)
// and dWater (factor between 0 and 1)
/*!
 * @brief Calculate Dwater and transpiration
 *
 * @param[out] trans transpiration (cm H20 * day^-1)
 * @param[out] dWater reduction in photosynthesis due to soil dryness (unitless,
 * [0:1])
 * @param[in] potGrossPsn potential photosynthesis before including dWater (g C
 * * m^-2 ground area * day^-1)
 * @param[in] vpd vapor pressure deficit (kPa)
 * @param[in] soilWater current water in soil (g C * m^-2 ground area)
 */
void moisture(double *trans, double *dWater, double potGrossPsn, double vpd,
              double soilWater) {
  // potential transpiration in the absense of plant water stress
  // (cm H20 * day^-1)
  double potTrans;
  // amount of water available for photosynthesis
  double removableWater;
  // water use efficiency, in mg CO2 fixed * g^-1 H20 transpired
  double wue;

  if (potGrossPsn < TINY) {  // avoid divide by 0
    *trans = 0.0;  // no photosynthesis -> no transpiration
    *dWater = 1;  // dWater doesn't matter, since we don't have any
                  // photosynthesis
  } else {
    // :: from [1], eq (A13)
    wue = params.wueConst / vpd;

    // 1000 converts g to mg; 44/12 converts g C to g CO2, 1/10000 converts m^2
    // to cm^2
    // :: from [1], eq (A14) plus unit conversion as described there
    potTrans = potGrossPsn / wue * 1000.0 * (44.0 / 12.0) * (1.0 / 10000.0);

    // :: from [1], discussion below eq (A14)
    removableWater = soilWater * params.waterRemoveFrac;

    // :: from [2], snowpack modification
    if (climate->tsoil < params.frozenSoilThreshold) {
      // frozen soil - less or no water available
      // frozen soil effect: fraction of water available if soil is frozen
      // (assume amount of water available in frozen soil scales linearly
      // with amount of water available in thawed soil)
      removableWater *= params.frozenSoilEff;
    }

    // :: from [1], eq (A15)
    *trans = fmin(removableWater, potTrans);

    // :: from [1], eq (A16)
    *dWater = *trans / potTrans;
  }
}

// have we passed the growing season-start leaf growth trigger this year?
// 0 = no, 1 = yes
// note: there may be some fluctuations in this signal for some methods of
// determining growing season start (e.g. for soil temp-based leaf growth)
int pastLeafGrowth(void) {
  if (ctx.gdd) {
    // :: from [1], description on pg 350
    // null pointer dereference warning suppressed on the next line
    return (climate->gdd >= params.gddLeafOn);  // NOLINT
  } else if (ctx.soilPhenol) {
    // [TAG:UNKNOWN_PROVENANCE] soil phenol functionality
    return (climate->tsoil >= params.soilTempLeafOn);  // soil temperature
                                                       // threshold
  } else {
    // :: from [1]
    double currTime = (double)climate->day + climate->time / 24.0;
    return (currTime >= params.leafOnDay);  // turn-on day
  }
}

// have we passed the growing season-end leaf fall trigger this year?
// 0 = no, 1 = yes
int pastLeafFall(void) {
  // :: from [1]
  return ((climate->day + climate->time / 24.0) >=
          params.leafOffDay);  // turn-off
                               // day
}

/*!
 * Calculate leaf creation and leaf litter fluxes
 *
 * Leaf creation is a fraction of recent mean npp, plus some constant amount
 * at start of growing season. Leaf litter is a constant rate, plus some
 * additional fraction of leaves at end of growing season.
 *
 * Mechanics of growing season boundary effects are from [1], modified to be a
 * fraction of total leaves growing/falling to allow for continual growth and
 * litter as described in [2], Appendix: Model Description. Calculation of
 * leafCreation is not from [1] (source TBD).
 *
 * @param[out] leafCreation (g C/m^2 ground/day)
 * @param[out] leafLitter (g C/m^2 ground/day)
 * @param[in] plantLeafC (g C/m^2 ground area)
 */
void calcLeafFluxes(double *leafCreation, double *leafLitter,
                    double plantLeafC) {
  // [TAG:UNKNOWN_PROVENANCE] leaf phenology combination
  // This function's exact source is still unknown, but is likely a combo of:
  // [1]: growing season boundary effects, but modified to be partial growth
  // and fall, instead of binary
  // [2]: leaf creation relationship with NPP

  double npp;  // temporal mean of recent npp (g C * m^-2 ground * day^-1)

  npp = getMeanTrackerMean(meanNPP);
  // first determine the fluxes that happen at every time step, not just start &
  // end of growing season:
  if (npp > 0) {
    // a fraction of NPP is allocated to leaf growth
    *leafCreation = npp * params.leafAllocation;
  } else {
    // net loss of C in this time step - no C left for growth
    *leafCreation = 0;
  }
  // a constant fraction of leaves fall in each time step
  *leafLitter = plantLeafC * params.leafTurnoverRate;

  // now add additional fluxes at start/end of growing season:

  // first check for new year; if new year, reset trackers (since we haven't
  // done leaf growth or fall yet in this new year):
  if (climate->year > phenologyTrackers.lastYear) {  // HAPPY NEW YEAR!
    phenologyTrackers.didLeafGrowth = 0;
    phenologyTrackers.didLeafFall = 0;
    phenologyTrackers.lastYear = climate->year;
  }

  // check for start of growing season:
  if (!phenologyTrackers.didLeafGrowth && pastLeafGrowth()) {
    // we just reached the start of the growing season
    *leafCreation += (params.leafGrowth / climate->length);
    phenologyTrackers.didLeafGrowth = 1;
  }

  // check for end of growing season:
  if (!phenologyTrackers.didLeafFall && pastLeafFall()) {
    // we just reached the end of the growing season
    *leafLitter += (plantLeafC * params.fracLeafFall) / climate->length;
    phenologyTrackers.didLeafFall = 1;
  }
}

// following 4 functions are for complex water sub-model

// calculate total rain and snowfall (cm water equiv./day)
// also, immediate evaporation (from interception) (cm/day)
void calcPrecip(double *rain, double *snowFall, double *immedEvap, double lai) {
  // below freezing -> precip falls as snow
  if (climate->tair <= 0) {
    *snowFall = climate->precip / climate->length;
    *rain = 0;
  }

  // above freezing -> precip falls as rain
  else {
    *snowFall = 0;
    *rain = climate->precip / climate->length;
  }

  /* Immediate evaporation is a sum of evaporation from canopy interception
     and evaporation from pools on the ground
     Higher LAI will mean more canopy evap. but less evap. from pools on the
     ground. For now we'll assume that these two effects cancel, and immediate
     evap. is a constant fraction Note that we don't evaporate snow here (we'll
     let sublimation take care of that)
  */

  if (ctx.leafWater) {
    double maxLeafPool;

    // calculate current leaf pool size depending on lai
    maxLeafPool = lai * params.leafPoolDepth;
    *immedEvap = (*rain) * params.immedEvapFrac;

    // don't evaporate more than pool size, excess water will go to the soil
    if (*immedEvap > maxLeafPool)
      *immedEvap = maxLeafPool;
  } else {
    *immedEvap = (*rain) * params.immedEvapFrac;
  }
}

// snowpack dynamics:
// calculate snow melt (cm water equiv./day) & sublimation (cm water equiv./day)
// ensure we don't overdrain the snowpack (so that it becomes negative)
// snowFall in cm/day
void snowPack(double *snowMelt, double *sublimation, double snowFall) {
  // conversion factor for sublimation
  static const double CONVERSION = (RHO * CP) / GAMMA * (1. / LAMBDA_S) *
                                   1000. * 1000. * (1. / 10000) * SEC_PER_DAY;
  // 1000 converts kg to g, 1000 converts kPa to Pa, 1/10000 converts m^2 to
  // cm^2

  double rd;  // aerodynamic resistance between ground and canopy air space
              // (sec/m)
  double snowRemaining;  // to make sure we don't get rid of more than there is

  // if no snow, set fluxes to 0
  if (envi.snow <= 0) {
    *snowMelt = 0;
    *sublimation = 0;
  }

  // else: there is snow
  else {
    // first calculate sublimation, then snow melt
    // (if there's not enough snow to do both, priority given to sublimation)
    rd = (params.rdConst) / (climate->wspd);  // aerodynamic resistance (sec/m)
    *sublimation = CONVERSION * (E_STAR_SNOW - climate->vPress) / rd;

    snowRemaining = envi.snow + (snowFall * climate->length);

    // remove to allow sublimation of a negative amount of snow
    // right now we can't sublime a negative amount of snow
    if (*sublimation < 0) {
      *sublimation = 0;
    }

    // make sure we don't sublime more than there is to sublime:
    if (snowRemaining - (*sublimation * climate->length) < 0) {
      *sublimation = snowRemaining / climate->length;
      snowRemaining = 0;
    }

    else {
      snowRemaining -= (*sublimation * climate->length);
    }

    // below freezing: no snow melt
    if (climate->tair <= 0) {
      *snowMelt = 0;
    }

    // above freezing: melt snow
    else {
      *snowMelt = params.snowMelt * climate->tair;  // snow melt proportional to
                                                    // temp.

      // make sure we don't melt more than there is to melt:
      if (snowRemaining - (*snowMelt * climate->length) < 0) {
        *snowMelt = snowRemaining / climate->length;
      }
    }  // end else above freezing
  }  // end else there is snow
}  // end snowPack

/*!
 *  Calculate fastFlow, evaporation, and drainage from soil
 *
 *  Note: irrigation moisture additions are calculated after this function is
 *  called, so any such additions will affect the NEXT climate step
 *  calculations.
 *
 * @param[out] fastFlow water that immediately goes to drainage (cm/day)
 * @param[out] evaporation water that evaporates fro msoil layer (cm/day)
 * @param[out] drainage water that drains from the soil (cm/day)
 * @param[in] water current water in soil layer
 * @param[in] netRain rain available to enter soil (cm/day)
 * @param[in] snowMelt water available from snow (cm equiv/day)
 * @param[in] trans water leaving via transpiration
 */
void calcSoilWaterFluxes(double *fastFlow, double *evaporation,
                         double *drainage, double water, double netRain,
                         double snowMelt, double trans) {
  // conversion factor for evaporation
  // 1000 converts kg to g, 1000 converts kPa to Pa, 1/10000 converts m^2 to
  // cm^2
  static const double CONVERSION = (RHO * CP) / GAMMA * (1. / LAMBDA) * 1000. *
                                   1000. * (1. / 10000) * SEC_PER_DAY;
  // keep running total of water remaining, in cm to make sure we don't
  // evaporate or drain too much, and so we can drain any overflow
  double waterRemaining;
  // keep track of net water into soil, in cm/day
  double netIn;
  // aerodynamic resistance between ground and canopy air space (sec/m)
  double rd;
  // bare soil surface resistance (sec/m)
  double rsoil;

  netIn = netRain + snowMelt;

  // fast flow: fraction that goes directly to drainage
  *fastFlow = netIn * params.fastFlowFrac;
  netIn -= *fastFlow;

  // calculate evaporation:
  // first calculate how much water is left to evaporate (used later)
  waterRemaining = water + netIn * climate->length - trans * climate->length;

  // if there's a snow pack, don't evaporate from soil:
  if (envi.snow > 0) {
    *evaporation = 0;
  }

  // else no snow pack:
  else {
    double waterFrac = water / params.soilWHC;
    rsoil = exp(params.rSoilConst1 - params.rSoilConst2 * (waterFrac));
    rd = (params.rdConst) / (climate->wspd);  // aerodynamic resistance (sec/m)
    *evaporation = CONVERSION * climate->vpdSoil / (rd + rsoil);
    // by using vpd we assume that relative humidity of soil pore space is 1
    // (when this isn't true, there won't be much water evaporated anyway)

    // remove to allow negative evaporation (i.e. condensation):
    if (*evaporation < 0) {
      *evaporation = 0;
    }

    // make sure we don't evaporate more than we have:
    if (waterRemaining - (*evaporation * climate->length) < TINY) {
      // leave a tiny little bit, to avoid negative water due to round-off
      // errors
      *evaporation = (waterRemaining - TINY) / climate->length;
      waterRemaining = 0;
    } else {
      waterRemaining -= (*evaporation * climate->length);
    }
  }

  // drain any water that remains beyond water holding capacity:
  if (waterRemaining > params.soilWHC) {
    *drainage = (waterRemaining - params.soilWHC) / (climate->length);
  } else {
    *drainage = 0;
  }
}

// calculate GROSS photosynthesis (g C * m^-2 * day^-1)
void getGpp(double *gpp, double potGrossPsn, double dWater) {
  // :: from [1], eq (A17)
  *gpp = potGrossPsn * dWater;
}

/*!
 * @brief Calculate foliar respiration and wood maintenance resp
 *
 * Does NOT explicitly calculate growth respiration; growth resp included in
 * wood maintenance resp.
 *
 * @param[out] folResp foliar respiration (g C * m^-2 ground area * day^-1)
 * @param[out] woodResp wood maintenance respiration (g C * m^-2 ground area *
 * day^-1)
 * @param[in] baseFolResp base foliar respiration (g C * m^-2 ground area *
 * day^-1)
 */
void vegResp(double *folResp, double *woodResp, double baseFolResp) {
  // Respiration model according to [1]

  // :: from [1], eq (A18)
  *folResp = baseFolResp *
             pow(params.vegRespQ10, (climate->tair - params.psnTOpt) / 10.0);

  // :: from [2], snowpack addition
  if (climate->tsoil < params.frozenSoilThreshold) {
    // allows foliar resp. to be shutdown by a given fraction in winter
    *folResp *= params.frozenSoilFolREff;
  }
  // end snowpack addition

  // :: from [1], eq (A19)
  *woodResp = params.baseVegResp * envi.plantWoodC *
              pow(params.vegRespQ10, climate->tair / 10.0);
}

// calculate foliar respiration and wood maint. resp, both in g C * m^-2 ground
// area * day^-1 does *not* explicitly model growth resp. (includes it in maint.
// resp)
void calcRootResp(double *rootResp, double respQ10, double baseRate,
                  double poolSize) {
  // :: from [3], root model description (eq (1) with root params)
  *rootResp = baseRate * poolSize * pow(respQ10, climate->tsoil / 10.0);
}

// a second veg. resp. method:
// calculate foliar resp., wood maint. resp. and growth resp., all in g C * m^-2
// ground area * day^-1 growth resp. modeled in a very simple way
// This is in addition to [1], source not yet determined (controlled by
// ctx.growthResp)
void vegResp2(double *folResp, double *woodResp, double *growthResp,
              double baseFolResp) {
  // [TAG:UNKNOWN_PROVENANCE] growthResp
  *folResp = baseFolResp *
             pow(params.vegRespQ10, (climate->tair - params.psnTOpt) / 10.0);
  if (climate->tsoil < params.frozenSoilThreshold) {
    *folResp *= params.frozenSoilFolREff;  // allows foliar resp. to be shutdown
                                           // by a given fraction in winter
  }
  *woodResp = params.baseVegResp * envi.plantWoodC *
              pow(params.vegRespQ10, climate->tair / 10.0);

  // Rg is a fraction of the recent mean NPP
  *growthResp = params.growthRespFrac * getMeanTrackerMean(meanNPP);

  if (*growthResp < 0) {
    *growthResp = 0;
  }
}

/////////////////

// ensure that all the allocation to wood + leaves + fine roots < 1,
// and calculate coarse root allocation
void ensureAllocation(void) {
  // :: from [3], root model description
  params.coarseRootAllocation = 1 - params.leafAllocation -
                                params.woodAllocation -
                                params.fineRootAllocation;

  if ((params.leafAllocation >= 1.0) || (params.woodAllocation >= 1.0) ||
      (params.fineRootAllocation >= 1.0) || (params.coarseRootAllocation < 0)) {
    printf("ERROR: NPP allocation params must be less than one individually "
           "and add to less than one\n");
    exit(EXIT_CODE_BAD_PARAMETER_VALUE);
  }
}

/*!
 *  Calculate moisture effect on soil respiration
 *
 *  Depends on soil temperature and whether waterHResp is on.
 *
 * @param water current soil water
 * @param whc water holding capacity
 * @return moisture effect as a fraction between 0 and 1
 */
double calcMoistEffect(double water, double whc) {
  double moistEffect;

  if ((!ctx.waterHResp) || (climate->tsoil < 0)) {
    // if not waterHResp, soil moisture does not affect heterotrophic
    // respiration; else:
    // :: from [2], snowpack addition
    moistEffect = 1.0;  // Ignore moisture effects in frozen soils
  } else {
    // :: from [1], first part of eq (A20), with added exponent
    // Original formulation from [1], based on PnET is:
    //   moistEffect = water / whc
    // which matches here with params.soilRespMoistEffect=1 (the default
    // value)
    //
    // [TAG:UNKNOWN_PROVENANCE] soilRespMoistEffect
    // Note: older versions of sipnet note this as "using PnET formulation",
    // but we have been unable to verify that the exponent comes from PnET
    moistEffect = pow((water / whc), params.soilRespMoistEffect);
  }

  return moistEffect;
}

/**
 *
 */
double calcTempEffect(double tsoil) {
  // :: from [1], D_temp calc as part of eq (A20)
  return params.baseSoilResp * pow(params.soilRespQ10, tsoil / 10);
}

/*!
 * Calculate the rSoil flux when microbes is off
 *
 * @param tsoil
 * @param water
 * @param whc
 */
void calcSoilMaintRespiration(double tsoil, double water, double whc) {

  // TBD We seem to be conflating maintResp and rSoil in the non-microbe
  // case, need to dig in. With that said...

  if (!ctx.microbes) {
    double moistEffect = calcMoistEffect(water, whc);

    // :: from [1], remainder of eq (A20)
    // See calcMoistEffect() for first part of eq (A20) calculation
    double tempEffect = calcTempEffect(tsoil);

    // Effects of tillage, if any
    double tillageEffect = 1 + eventTrackers.d_till_mod;

    // Put it all together!
    fluxes.maintRespiration =
        envi.soil * moistEffect * tempEffect * tillageEffect;

    // With no microbes, rSoil flux is just the maintenance respiration
    fluxes.rSoil = fluxes.maintRespiration;
  }
  // else fluxes.rSoil = 0.0?
}

/*!
 * Calculates fluxes used when modeling microbes
 */
void calcMicrobeFluxes(double tsoil, double water, double whc,
                       double rootExudate) {
  if (ctx.microbes) {
    double baseRate;
    double tempEffect;
    double moistEffect = calcMoistEffect(water, whc);

    // :: from [4], part of eqs (5.9) and (5.10)
    //    Calculates mu_max * C_B * g(C_S), to be used to calculate both
    //    eqs (5.9) and (5.10) in updatePoolsForSoil()

    // :: mu_max * g(C_S)
    baseRate = params.maxIngestionRate * envi.soil /
               (params.halfSatIngestion + envi.soil);

    // Flux that microbes remove from soil  (mg C g soil day)
    // Some is ingested, rest used for growth in updatePoolsForSoil()
    // :: above * C_B
    fluxes.microbeIngestion = baseRate * envi.microbeC;

    // fluxes that get added to microbe pool
    // :: from [4], eq (5.11) first line
    fluxes.soilPulse = params.microbePulseEff * (rootExudate);

    // respiration is determined by microbe biomass
    // :: from [4], eq (5.12) with addition of moisture effect
    // [TAG:UNKNOWN_PROVENANCE]  moistEffect
    tempEffect = params.baseMicrobeResp * pow(params.microbeQ10, tsoil / 10);
    fluxes.maintRespiration = envi.microbeC * moistEffect * tempEffect;

  } else {
    fluxes.microbeIngestion = 0.0;
    fluxes.soilPulse = 0.0;
    // fluxes.maintRespiration is otherwise set, do not set to zero here
  }
}

void calcLitterFluxes() {
  if (ctx.litterPool) {
    double tempEffect = pow(params.soilRespQ10, climate->tsoil / 10.0);
    double moistEffect = calcMoistEffect(envi.soilWater, params.soilWHC);
    // total litter breakdown (i.e. litterToSoil + rLitter) (g C/m^2 ground/day)
    double litterBreakdown =
        envi.litter * params.litterBreakdownRate * tempEffect * moistEffect;

    fluxes.rLitter = litterBreakdown * params.fracLitterRespired;
    fluxes.litterToSoil = litterBreakdown * (1.0 - params.fracLitterRespired);
  } else {
    // litterBreakdown = 0;
    fluxes.rLitter = 0;
    fluxes.litterToSoil = 0;
  }
}

/*!
 * Calculate root and wood creation and loss
 *
 * @return total root exudate for use with microbe model
 */
double calcRootAndWoodFluxes(void) {
  double coarseExudate, fineExudate;  // exudates in and out of soil
  double npp, gppSoil;  // running means of our tracker variables

  npp = getMeanTrackerMean(meanNPP);
  gppSoil = getMeanTrackerMean(meanGPP);
  if (npp > 0) {
    // :: from [3], root model description and eq (3)
    fluxes.coarseRootCreation = params.coarseRootAllocation * npp;
    fluxes.fineRootCreation = params.fineRootAllocation * npp;
    fluxes.woodCreation = params.woodAllocation * npp;
  } else {
    fluxes.coarseRootCreation = 0;
    fluxes.fineRootCreation = 0;
    fluxes.woodCreation = 0;
  }

  if ((gppSoil > 0) & (envi.fineRootC > 0)) {
    // :: from [3], eq (5)
    coarseExudate = params.coarseRootExudation * gppSoil;
    fineExudate = params.fineRootExudation * gppSoil;
  } else {
    fineExudate = 0;
    coarseExudate = 0;
  }

  // :: from [3], roots model descriptions and [4] eq (5.11)
  fluxes.coarseRootLoss = (1 - params.microbePulseEff) * coarseExudate +
                          params.coarseRootTurnoverRate * envi.coarseRootC;
  fluxes.fineRootLoss = (1 - params.microbePulseEff) * fineExudate +
                        params.fineRootTurnoverRate * envi.fineRootC;

  // Wood litter, in g C * m^-2 ground area * day^-1
  // turnover rate is fraction lost per day
  fluxes.woodLitter = envi.plantWoodC * params.woodTurnoverRate;

  // :: from [3], root model description
  calcRootResp(&fluxes.rCoarseRoot, params.coarseRootQ10,
               params.baseCoarseRootResp, envi.coarseRootC);
  calcRootResp(&fluxes.rFineRoot, params.fineRootQ10, params.baseFineRootResp,
               envi.fineRootC);

  return fineExudate + coarseExudate;
}

/*!
 * Calculate mineral N volatilization flux
 */
void calcNVolatilizationFlux() {
  // flux = k_vol * nMin * Dtemp * Dwater
  double d_temp = calcTempEffect(climate->tsoil);
  double d_water = calcMoistEffect(envi.soilWater, params.soilWHC);

  fluxes.nVolatilization =
      (params.nVolatilization * envi.minN * d_temp * d_water) / climate->length;
}

/*!
 * Calculate mineral N leaching flux
 */
void calcNLeachingFlux() {
  double phi;
  // phi is (drainage / soilWHC) between 0 and 1
  if ((fluxes.drainage / params.soilWHC) < 1) {
    phi = fluxes.drainage / params.soilWHC;
  }
  else {
    phi = 1;
  }
  // flux = nMin * phi * leaching fraction, g N * m^-2 * day^-1
  fluxes.nLeaching = envi.minN * phi * params.nLeachingFrac;
}

/*!
 * Calculate flux terms for sipnet as part of main model flow
 *
 * All fluxes should be calculated before state variables are updated.
 */
void calculateFluxes(void) {
  // base foliar respiration, calc'd as part of potential photosynthesis
  double baseFolResp;
  // potential photosynthesis, without water stress
  double potGrossPsn;
  // reduction in photosynthesis due to soil dryness
  double dWater;
  // m^2 leaf/m^2 ground (calculated from plantLeafC)
  double lai;
  // maintenance respiration terms (g C * m^-2 ground area * day^-1)
  double folResp, woodResp;
  // Growth respiration, if splitting growth and maintenance
  // (g C * m^-2 ground area * day^-1)
  double growthResp;
  // net rain, equal to (rain - immedEvap) (cm/day)
  double netRain;

  // Psn, moisture and water fluxes
  lai = envi.plantLeafC / params.leafCSpWt;  // current lai

  potPsn(&potGrossPsn, &baseFolResp, lai, climate->tair, climate->vpd,
         climate->par);
  moisture(&(fluxes.transpiration), &dWater, potGrossPsn, climate->vpd,
           envi.soilWater);
  calcPrecip(&(fluxes.rain), &(fluxes.snowFall), &(fluxes.immedEvap), lai);
  netRain = fluxes.rain - fluxes.immedEvap;
  snowPack(&(fluxes.snowMelt), &(fluxes.sublimation), fluxes.snowFall);
  calcSoilWaterFluxes(&(fluxes.fastFlow), &(fluxes.evaporation),
                      &(fluxes.drainage), envi.soilWater, netRain,
                      fluxes.snowMelt, fluxes.transpiration);
  getGpp(&(fluxes.photosynthesis), potGrossPsn, dWater);

  // Vegetation respiration
  if (ctx.growthResp) {
    vegResp2(&folResp, &woodResp, &growthResp, baseFolResp);
    fluxes.rVeg = folResp + woodResp + growthResp;
  } else {
    vegResp(&folResp, &woodResp, baseFolResp);
    fluxes.rVeg = folResp + woodResp;
  }

  // Leaf creation and litter
  calcLeafFluxes(&(fluxes.leafCreation), &(fluxes.leafLitter), envi.plantLeafC);

  // Litter pool, if LITTER is on
  calcLitterFluxes();

  // Roots and microbes
  double rootExudate = calcRootAndWoodFluxes();

  // Microbes
  if (ctx.microbes) {
    calcMicrobeFluxes(climate->tsoil, envi.soilWater, params.soilWHC,
                      rootExudate);
  } else {
    calcSoilMaintRespiration(climate->tsoil, envi.soilWater, params.soilWHC);
  }

  // Nitrogen cycle
  if (ctx.nitrogenCycle) {
    calcNVolatilizationFlux();
    // Leaching depends on drainage flux so makes sure calcNLeachingFlux
    // occurs after calcSoilWaterFluxes
    calcNLeachingFlux();
  }
}

// !!! functions for updating tracker variables !!!

// initialize trackers at start of simulation:
void initTrackers(void) {
  trackers.gpp = 0.0;
  trackers.rtot = 0.0;
  trackers.ra = 0.0;
  trackers.rh = 0.0;
  trackers.npp = 0.0;
  trackers.nee = 0.0;
  trackers.yearlyGpp = 0.0;
  trackers.yearlyRtot = 0.0;
  trackers.yearlyRa = 0.0;
  trackers.yearlyRh = 0.0;
  trackers.yearlyNpp = 0.0;
  trackers.yearlyNee = 0.0;
  trackers.totGpp = 0.0;
  trackers.totRtot = 0.0;
  trackers.totRa = 0.0;
  trackers.totRh = 0.0;
  trackers.totNpp = 0.0;
  trackers.totNee = 0.0;
  trackers.evapotranspiration = 0.0;
  trackers.soilWetnessFrac = envi.soilWater / params.soilWHC;
  trackers.rSoil = 0.0;
  trackers.woodCreation = 0.0;

  trackers.rRoot = 0.0;

  trackers.rAboveground = 0.0;

  trackers.yearlyLitter = 0.0;
}

// If var < minVal, then set var = 0
// Note that if minVal = 0, then this will (as suggested) ensure that var >= 0
//  If minVal > 0, then minVal can be thought of as some epsilon value, below
//  which var is treated as 0
void ensureNonNegative(double *var, double minVal) {
  if (*var < minVal) {
    *var = 0.;
  }
}

// Make sure all environment variables are positive after updating them:
// Note: For some variables, there are checks elsewhere in the code to ensure
// that fluxes don't make stocks go negative
//  However, the stocks could still go slightly negative due to rounding errors
// For other variables, there are NOT currently (as of 7-16-06) checks to make
// sure out-fluxes aren't too large
//  In these cases, this function should be thought of as a last-resort check -
//  ideally, the fluxes would be modified so that they did not make the stocks
//  negative (otherwise the fluxes could be inconsistent with the changes in the
//  stocks)
void ensureNonNegativeStocks(void) {

  ensureNonNegative(&(envi.plantWoodC), 0);
  ensureNonNegative(&(envi.plantLeafC), 0);

  if (ctx.litterPool) {
    ensureNonNegative(&(envi.litter), 0);
  }

  ensureNonNegative(&(envi.soil), 0);
  ensureNonNegative(&(envi.coarseRootC), 0);
  ensureNonNegative(&(envi.fineRootC), 0);
  ensureNonNegative(&(envi.microbeC), 0);
  ensureNonNegative(&(envi.soilWater), 0);

  /* In the case of snow, the model has very different behavior for a snow pack
     of 0 vs. a snow pack of slightly greater than 0 (e.g. no soil evaporation
     if snow > 0). Thus, to avoid large errors due to small rounding errors,
     we'll set snow = 0 any time it falls below TINY, the assumption being that
     if snow < TINY, then it was really supposed to be 0, but isn't because of
     rounding errors.*/
  ensureNonNegative(&(envi.snow), TINY);
}

// update trackers at each time step
// oldSoilWater is how much soil water there was at the beginning of the time
// step (cm)
void updateTrackers(double oldSoilWater) {
  static int lastYear = -1;  // what was the year of the last step?

  if (climate->year != lastYear) {  // new year: reset yearly trackers
    trackers.yearlyGpp = 0.0;
    trackers.yearlyRtot = 0.0;
    trackers.yearlyRa = 0.0;
    trackers.yearlyRh = 0.0;
    trackers.yearlyNpp = 0.0;
    trackers.yearlyNee = 0.0;

    lastYear = climate->year;
  }

  trackers.gpp = fluxes.photosynthesis * climate->length;

  // everything that is microbial
  trackers.rh = (fluxes.rLitter + fluxes.rSoil) * climate->length;

  // This is wood plus leaf respiration
  trackers.rAboveground = (fluxes.rVeg) * climate->length;

  trackers.rRoot = (fluxes.rCoarseRoot + fluxes.rFineRoot) * climate->length;
  trackers.rSoil = trackers.rRoot + trackers.rh;
  trackers.ra = trackers.rRoot + trackers.rAboveground;
  trackers.rtot = trackers.ra + trackers.rh;
  trackers.npp = trackers.gpp - trackers.ra;
  trackers.nee = -1.0 * (trackers.npp - trackers.rh);

  trackers.yearlyGpp += trackers.gpp;
  trackers.yearlyRa += trackers.ra;
  trackers.yearlyRh += trackers.rh;
  trackers.yearlyRtot += trackers.rtot;
  trackers.yearlyNpp += trackers.npp;
  trackers.yearlyNee += trackers.nee;

  trackers.totGpp += trackers.gpp;
  trackers.totRa += trackers.ra;
  trackers.totRh += trackers.rh;
  trackers.totRtot += trackers.rtot;
  trackers.totNpp += trackers.npp;
  trackers.totNee += trackers.nee;
  trackers.woodCreation = fluxes.woodCreation * climate->length;

  // evapotranspiration includes water lost to evaporation from canopy
  // irrigation (fluxes.eventEvap)
  trackers.evapotranspiration =
      (fluxes.transpiration + fluxes.immedEvap + fluxes.evaporation +
       fluxes.sublimation + fluxes.eventEvap) *
      climate->length;

  trackers.soilWetnessFrac =
      (oldSoilWater + envi.soilWater) / (2.0 * params.soilWHC);

  trackers.yearlyLitter += fluxes.leafLitter;
}

void updateMeanTrackers(void) {
  double npp;  // net primary productivity, g C * m^-2 ground area * day^-1
  int err;

  npp = fluxes.photosynthesis - fluxes.rVeg - fluxes.rCoarseRoot -
        fluxes.rFineRoot;

  err = addValueToMeanTracker(meanNPP, npp, climate->length);  // update running
                                                               // mean of NPP
  if (err != 0) {
    printf("******* Error type %d while trying to add value to NPP mean "
           "tracker in sipnet:updateState() *******\n",
           err);
    printf("npp = %f, climate->length = %f\n", npp, climate->length);
    printf("Suggestion: try changing MEAN_NPP_MAX_ENTRIES in sipnet.c\n");
    exit(1);
  }

  err = addValueToMeanTracker(meanGPP, fluxes.photosynthesis,
                              climate->length);  // update running mean of GPP
  if (err != 0) {
    printf("******* Error type %d while trying to add value to GPP mean "
           "tracker in sipnet:updateState() *******\n",
           err);
    printf("GPP = %f, climate->length = %f\n", fluxes.photosynthesis,
           climate->length);
    printf("Suggestion: try changing MEAN_GPP_SOIL_MAX_ENTRIES in sipnet.c\n");
    exit(1);
  }
}

/*!
 * Update the main pools, leafC, woodC, soil and snow
 */
void updateMainPools() {
  // Update the stocks, with fluxes adjusted for length of time step.
  // Notes:
  // - GPP shows up twice (direct + via NPP --> woodCreation), but
  //   the math works out to:
  //     envi.plantWoodC += NPP_allocation_to_wood − woodLitter.
  // - The soil C pool(s) (envi.soil, envi.fineRootC, envi.CoarseRootC)
  //   are updated in updatePoolsForSoil().

  // :: from [1], eq (A1), where:
  //     GPP = fluxes.photosynthesis
  //     R_a = fluxes.rVeg
  //     L_w = fluxes.woodLitter
  //     L   = fluxes.leafCreation
  // :: from [3], additional root terms (woodCreation, fineRootC, coarseRootC)
  //    from NPP allocation
  envi.plantWoodC += (fluxes.photosynthesis + fluxes.woodCreation -
                      fluxes.leafCreation - fluxes.woodLitter - fluxes.rVeg -
                      fluxes.coarseRootCreation - fluxes.fineRootCreation) *
                     climate->length;

  // :: from [1], eq (A2), where:
  //     L   = fluxes.leafCreation
  //     L_L = fluxes.leafLitter
  envi.plantLeafC +=
      (fluxes.leafCreation - fluxes.leafLitter) * climate->length;

  // :: from [1], eq (A4), where:
  //     P = (fluxes.rain - fluxes.immedEvap)
  //     T = (fluxes.transpiration + fluxes.evaporation)
  //     D = (fluxes.bottomDrainage + fluxes.fastFlow)
  //    from [2], addition of fluxes.snowMelt, as well as the modifications in
  //    the terms above: immedEvap in P, evaporation in T, and fastFlow in D
  envi.soilWater +=
      (fluxes.rain + fluxes.snowMelt - fluxes.immedEvap - fluxes.fastFlow -
       fluxes.evaporation - fluxes.transpiration - fluxes.drainage) *
      climate->length;

  // if ctx.snow = 0, some or all of these fluxes will always be 0
  // :: from [2], addition of snowpack description
  envi.snow += (fluxes.snowFall - fluxes.snowMelt - fluxes.sublimation) *
               climate->length;
}

/*!
 * Calculate soil respiration flux and update carbon pools
 *
 * Calculates soil respiration, method depending on the MICROBES and
 * LITTER_POOL flags. Updates carbon pools for soil, fine roots,
 * and coarse roots.
 *
 * TODO: split this apart - fluxes into calculateFluxes, with pool updates
 *       after, as is the general method for SIPNET.
 */
void updatePoolsForSoil(void) {

  // UPDATE POOLS
  if (ctx.microbes) {
    double microbeEff = params.efficiency;

    // :: from [1] for litter terms
    // :: from [3] for root terms
    // :: from [4] for microbeIngestion term
    // Note: no rSoil term here, as soil resp is handled by microbeIngestion
    envi.soil +=
        (fluxes.coarseRootLoss + fluxes.fineRootLoss + fluxes.woodLitter +
         fluxes.leafLitter - fluxes.microbeIngestion) *
        climate->length;
    // microbeC additions due to ingestion and incorporation of root exudates,
    // with reduction from microbe respiration
    // :: from [4], eq (5.9) for first (ingestion) term,
    // ::      eq (5.11) used for soilPulse, and
    // ::      eq (5.12) used for maintRespiration
    envi.microbeC += (microbeEff * fluxes.microbeIngestion + fluxes.soilPulse -
                      fluxes.maintRespiration) *
                     climate->length;

    // rSoil is maintenance resp + growth (microbe) resp
    // :: from [4], eq (5.10) for microbe term
    fluxes.rSoil =
        fluxes.maintRespiration + (1 - microbeEff) * fluxes.microbeIngestion;
  } else {
    if (ctx.litterPool) {
      // :: from [2], litter model description
      envi.litter += (fluxes.woodLitter + fluxes.leafLitter -
                      fluxes.litterToSoil - fluxes.rLitter) *
                     climate->length;

      // from [2] and [3], litter and root terms respectively
      envi.soil += (fluxes.coarseRootLoss + fluxes.fineRootLoss +
                    fluxes.litterToSoil - fluxes.rSoil) *
                   climate->length;
    } else {
      // Normal pool (single pool, no microbes)
      // :: from [1] (and others, TBD), eq (A3), where:
      //     L_w = fluxes.woodLitter
      //     L_l = fluxes.leafLitter
      //     R_h = fluxes.rSoil
      // :: from [3], root terms
      envi.soil += (fluxes.coarseRootLoss + fluxes.fineRootLoss +
                    fluxes.woodLitter + fluxes.leafLitter - fluxes.rSoil) *
                   climate->length;
    }
  }
  // :: from [3], root model description
  envi.coarseRootC +=
      (fluxes.coarseRootCreation - fluxes.coarseRootLoss - fluxes.rCoarseRoot) *
      climate->length;
  envi.fineRootC +=
      (fluxes.fineRootCreation - fluxes.fineRootLoss - fluxes.rFineRoot) *
      climate->length;

  // Nitrogen Cycle
  // Added for MAGIC project
  envi.minN -= (fluxes.nVolatilization + fluxes.nLeaching) * climate->length;
}

// !!! main runner function !!!

// calculate all fluxes and update state for this time step
// we calculate all fluxes before updating state in case flux calculations
// depend on the old state
void updateState(void) {
  // how much soil water was there before we updated it?
  // Used in trackers
  double oldSoilWater = envi.soilWater;

  ///////////////////////
  // 1. Calculate Fluxes

  // All non-event fluxes
  calculateFluxes();

  // All event handling, which is modeled as fluxes
  processEvents();

  ///////////////////////
  // 2. Update Pools

  // Update leafC, woodC, soil water and snow pools
  updateMainPools();

  // Update soil carbon pools
  updatePoolsForSoil();

  // Update pools for fluxes from events
  updatePoolsForEvents();

  // Verify none of our stocks have gone negative (set any that are to zero)
  ensureNonNegativeStocks();

  ///////////////////////
  // 3. Update trackers

  updateTrackers(oldSoilWater);

  updateMeanTrackers();

  updateEventTrackers();
}

// initialize phenology tracker structure, based on day of year of first climate
// record (have the leaves come on yet this year? have they fallen off yet this
// year?)
void initPhenologyTrackers(void) {

  phenologyTrackers.didLeafGrowth = pastLeafGrowth();  // first year: have we
                                                       // passed growing season
                                                       // start date?
  phenologyTrackers.didLeafFall = pastLeafFall();  // first year: have we passed
                                                   // growing season end date?

  /* if we think we've done leaf fall this year but not leaf growth, something's
     wrong this could happen if, e.g. we're using soil temp-based leaf growth,
     and soil temp. on first day doesn't happen to pass threshold
     fix this by setting didLeafGrowth to 1 in this case
     Note: this won't catch all potential problems (e.g. with soil temp-based
     leaf growth, temp. on first day may not pass threshold, but it may not yet
     be the end of the growing season; in this case we could accidentally grow
     the leaves again once the temp. rises past the threshold again. Also, we'll
     have problems with GDD-based growth if first day of simulation is not
     Jan. 1.)
  */
  if (phenologyTrackers.didLeafFall && !phenologyTrackers.didLeafGrowth) {
    phenologyTrackers.didLeafGrowth = 1;
  }
  // printf("stuff: %8d %8d \n",phenologyTrackers.lastYear, climate->year);
  phenologyTrackers.lastYear = climate->year;  // set the year of the previous
                                               // (non-existent) time step to be
                                               // this year
}

// See sipnet.h
void setupModel(void) {

  // a test: use constant (measured) soil respiration:
  // make it so soil resp. is 5.2 g C m-2 day-1 at 10 degrees C,
  // moisture-saturated soil, and soil C = init. soil C params.baseSoilResp =
  // ((5.2 * 365.0)/params.soilInit)/params.soilRespQ10;

  // ensure that all the allocation parameters sum up to something less than
  // one:
  ensureAllocation();

  ///
  /// PARAMS SETUP

  // If we aren't explicitly modeling microbe pool, then do not have a pulse to
  // microbes, exudates go directly to the soil
  if (!ctx.microbes) {
    params.microbePulseEff = 0;
  }

  // change units of parameters:
  params.baseVegResp /= 365.0;  // change from per-year to per-day rate
  params.litterBreakdownRate /= 365.0;
  params.baseSoilResp /= 365.0;
  params.woodTurnoverRate /= 365.0;
  params.leafTurnoverRate /= 365.0;

  // calculate additional parameters:
  params.psnTMax =
      params.psnTOpt + (params.psnTOpt - params.psnTMin);  // assumed
                                                           // symmetrical

  envi.plantWoodC =
      (1 - params.coarseRootFrac - params.fineRootFrac) * params.plantWoodInit;
  envi.plantLeafC = params.laiInit * params.leafCSpWt;

  if (ctx.litterPool) {
    envi.litter = params.litterInit;
  } else {
    // Don't set a value if the litter pool is off
    envi.litter = 0.0;
  }
  envi.soil = params.soilInit;

  // change from per hour to per day rate
  params.maxIngestionRate = params.maxIngestionRate * 24;

  if (ctx.microbes) {
    // convert to gC m-2
    envi.microbeC = params.microbeInit * params.soilInit / 1000;
  } else {
    // Don't set a value if microbes is off
    envi.microbeC = 0.0;
  }

  params.fineRootTurnoverRate /= 365.0;
  params.coarseRootTurnoverRate /= 365.0;

  params.baseCoarseRootResp /= 365.0;
  params.baseFineRootResp /= 365.0;
  params.baseMicrobeResp = params.baseMicrobeResp * 24;  // change from per hour
                                                         // to per day rate

  ///
  /// ENVIRONMENT SETUP

  envi.coarseRootC = params.coarseRootFrac * params.plantWoodInit;
  envi.fineRootC = params.fineRootFrac * params.plantWoodInit;

  envi.soilWater = params.soilWFracInit * params.soilWHC;
  if (envi.soilWater < 0) {
    envi.soilWater = 0;
  } else if (envi.soilWater > params.soilWHC) {
    envi.soilWater = params.soilWHC;
  }

  envi.snow = params.snowInit;

  if (ctx.nitrogenCycle) {
    envi.minN = params.minNInit;
  } else {
    envi.minN = 0.0;
  }

  climate = firstClimate;

  initTrackers();
  initPhenologyTrackers();
  initEventTrackers();
  resetMeanTracker(meanNPP, 0);  // initialize with mean NPP (over last
                                 // MEAN_NPP_DAYS) of 0
  resetMeanTracker(meanGPP, 0);  // initialize with mean NPP (over last
                                 // MEAN_GPP_DAYS) of 0
}

// See sipnet.h
void runModelOutput(FILE *out, OutputItems *outputItems, int printHeader) {
  if ((out != NULL) && printHeader) {
    outputHeader(out);
  }

  setupModel();
  setupEvents();

  while (climate != NULL) {
    updateState();
    if (out != NULL) {
      outputState(out, climate->year, climate->day, climate->time);
    }
    if (outputItems != NULL) {
      writeOutputItemValues(outputItems);
    }
    climate = climate->nextClim;
  }
  if (outputItems != NULL) {
    terminateOutputItemLines(outputItems);
  }
}

// See sipnet.h
void setupOutputItems(OutputItems *outputItems) {
  addOutputItem(outputItems, "NEE", &(trackers.nee));
  addOutputItem(outputItems, "NEE_cum", &(trackers.totNee));
  addOutputItem(outputItems, "GPP", &(trackers.gpp));
  addOutputItem(outputItems, "GPP_cum", &(trackers.totGpp));
}

// See sipnet.h
void initModel(ModelParams **modelParams, const char *paramFile,
               const char *climFile) {
  readParamData(modelParams, paramFile);
  readClimData(climFile);

  meanNPP = newMeanTracker(0, MEAN_NPP_DAYS, MEAN_NPP_MAX_ENTRIES);
  meanGPP = newMeanTracker(0, MEAN_GPP_SOIL_DAYS, MEAN_GPP_SOIL_MAX_ENTRIES);
}

// See sipnet.h
void cleanupModel() {
  freeClimateList();

  deallocateMeanTracker(meanNPP);
  deallocateMeanTracker(meanGPP);
  if (ctx.events) {
    freeEventList();
    closeEventOutFile();
  }
}

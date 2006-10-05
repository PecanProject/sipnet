/* SiPnET: Simplified PnET

   Author: Bill Sacks

   A simple box model of carbon cycling
   largely based on PnET
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "sipnet.h"
#include "runmean.h"
#include "util.h"
#include "spatialParams.h"

// begin definitions for choosing different model structures
// (1 -> true, 0 -> false)

#define GROWTH_RESP 0
// explicitly model growth resp., rather than including with maint. resp.

#define LITTER_POOL 0
// have extra litter pool, in addition to soil c pool

#define LLOYD_TAYLOR 0
// use Lloyd-Taylor model for soil respiration, in which temperature sensitivity decreases at higher temperatures?
// Lloyd-Taylor model is R = R0 * e^(E0 * (1/(-T0) - 1/(T - T0))) (see Lloyd and Taylor 1994, "On the temperature dependence of soil respiration") 
// where R0 is rate at 0 degrees C, T is soil temp., E0 and T0 are parameters

#define SEASONAL_R_SOIL 0 && !LLOYD_TAYLOR
// use different parameters for soil resp. (baseSoilResp and soilRespQ10) when tsoil < (some threshold)?
// if so, use standard parameters for warm soil, separate parameters for cold soil
// if we're using the Lloyd-Taylor model, we won't use different parameters at different temperatures

#define WATER_PSN 1
// does soil moisture affect photosynthesis?

#define WATER_HRESP 1
// does soil moisture affect heterotrophic respiration?

#define DAYCENT_WATER_HRESP 0 && WATER_HRESP
// use DAYCENT soil moisture function?

#define MODEL_WATER 1
// do we model soil water?
// if not, take soil wetness from climate file

#define COMPLEX_WATER 1 && MODEL_WATER
// do we use a more complex water submodel? (model evaporation as well as transpiration)
// when we use a complex water submodel, we always model snow
// if model water is off, then complex water is off: complex water wouldn't do anything

#define LITTER_WATER 0 && (COMPLEX_WATER)
// do we have a separate litter water layer, used for evaporation? 
// if complex water is off, then litter water is off: litter water layer wouldn't do anything

#define LITTER_WATER_DRAINAGE 1 && (LITTER_WATER)
// does water from the top layer drain down into bottom layer even if top layer not overflowing?
// if litter water is off, then litter water drainage is off: litter water drainage wouldn't do anything

#define SNOW (1 || (COMPLEX_WATER)) && MODEL_WATER
// keep track of snowpack, rather than assuming all precip. is liquid
// note: when using a complex water submodel, we ALWAYS keep track of snowpack
// if model water is off, then snow is off: snow wouldn't do anything

#define GDD 0
// use GDD to determine leaf growth? (note: mutually exclusive with SOIL_PHENOL)

#define SOIL_PHENOL 0 && !GDD
// use soil temp. to determine leaf growth? (note: mutually exclusive with GDD)

// end definitions for choosing different model structures

// begin constant definitions

#define C_WEIGHT 12.0 // molecular weight of carbon
#define TEN_6 1000000.0 // for conversions from micro
#define TEN_9 1000000000.0 // for conversions from nano
#define SEC_PER_DAY 86400.0

// constants for tracking running mean of NPP:
#define MEAN_NPP_DAYS 5 // over how many days do we keep the running mean?
#define MEAN_NPP_MAX_ENTRIES MEAN_NPP_DAYS * 24 // assume that the most pts we can have is one per hour

// some constants for water submodel:
#define LAMBDA 2501000. // latent heat of vaporization (J/kg)
#define LAMBDA_S 2835000. // latent heat of sublimation (J/kg)
#define RHO 1.3 // air density (kg/m^3)
#define CP 1005. // specific heat of air (J/(kg K))
#define GAMMA 66. // psychometric constant (Pa/K)
#define E_STAR_SNOW 0.6 /* approximate saturation vapor pressure at 0 degrees C (kPa)
			   (we assume snow temperature is 0 degrees C or slightly lower) */

#define TINY 0.000001 // to avoid those nasty divide-by-zero errors

// end constant definitions

// linked list of climate variables
// one node for each time step
// these climate data are read in from a file

typedef struct ClimateVars ClimateNode; 

struct ClimateVars {
  int year; // year of start of this timestep 
  int day; // day of start of this timestep (1 = Jan 1.)
  double time; // time of start of this timestep (hour.fraction - e.g. noon = 12.0, midnight = 0.0)
  double length; // length of this timestep (in days) - allow variable-length timesteps
  double tair; // avg. air temp for this time step (degrees C)
  double tsoil; // avg. soil temp for this time step (degrees C)
  double par; /* average par for this time step (Einsteins * m^-2 ground area * day^-1)
		NOTE: input is in Einsteins * m^-2 ground area, summed over entire time step */
  double precip; /* total precip. for this time step (cm water equiv. - either rain or snow)
		    NOTE: input is in mm */
  double vpd; /* average vapor pressure deficit (kPa)
		 NOTE: input is in Pa */
  double vpdSoil; /* average vapor pressure deficit between soil and air (kPa)
		     NOTE: input is in Pa
		     differs from vpd in that saturation vapor pressure calculated using Tsoil rather than Tair */
  double vPress; /* average vapor pressure in canopy airspace (kPa)
		    NOTE: input is in Pa */
  double wspd; // avg. wind speed (m/s)
  double soilWetness; // fractional soil wetness (fraction of saturation - between 0 and 1)

#if GDD
  double gdd; /* growing degree days from Jan. 1 to now
		 NOTE: Calculated, *not* read from file */
#endif

  ClimateNode *nextClim;
}; 


// model parameters which can change from one run to the next
// these include initializations of state
// initial values are read in from a file, or calculated at start of model
typedef struct Parameters {
  // parameters read in from file:
  // initial state values:
  double plantWoodInit; // g C * m^-2 ground area in wood (above-ground + roots)
  double laiInit; // initial leaf area, m^2 leaves * m^-2 ground area (multiply by leafCSpWt to get initial plant leaf C)
  double litterInit; // g C * m^-2 ground area
  double soilInit; // g C * m^-2 ground area
  double litterWFracInit; // unitless: fraction of litterWHC
  double soilWFracInit; // unitless: fraction of soilWHC
  double snowInit; // cm water equiv.

  // parameters:

  // photosynthesis:
  double aMax; /* max photosynthesis (nmol CO2 * g^-1 leaf * sec^-1)
		 assuming max. possible par, all intercepted, no temp, water or vpd stress */
  double aMaxFrac; // avg. daily aMax as fraction of instantaneous
  double baseFolRespFrac; // basal foliage resp. rate, as % of max. net photosynth. rate
  double psnTMin, psnTOpt; // min and optimal temps at which net photosynthesis occurs (degrees C)
  double dVpdSlope, dVpdExp; // dVpd = 1 - dVpdSlope * vpd^dVpdExp
  double halfSatPar; /*  par at which photosynthesis occurs at 1/2 theoretical maximum
		       (Einsteins * m^-2 ground area * day^-1) */
  double attenuation; // light attenuation coefficient
  
  // phenology-related:
  double leafOnDay; // day when leaves appear
  double gddLeafOn; // with gdd-based phenology, gdd threshold for leaf appearance
  double soilTempLeafOn; // with soil temp-based phenology, soil temp threshold for leaf appearance
  double leafOffDay; // day when leaves disappear
  double leafGrowth; // add'l leaf growth at start of growing season (g C * m^-2 ground)
  double fracLeafFall; // add'l fraction of leaves that fall at end of growing season
  double leafAllocation; // fraction of NPP allocated to leaf growth
  double leafTurnoverRate; /* average turnover rate of leaves, in fraction per day
			      NOTE: read in as per-year rate! */

  // autotrophic respiration:
  double baseVegResp; /* vegetation maintenance respiration at 0 degrees C
			(g C respired * g^-1 plant C * day^-1) 
			NOTE: only counts plant wood C - leaves handled elsewhere
			(both above and below-ground: assumed for now to have same resp. rate)
			NOTE: read in as per-year rate! */
  double vegRespQ10; // scalar determining effect of temp on veg. resp.
  double growthRespFrac; // growth resp. as fraction of (GPP - woodResp - folResp)
  double frozenSoilFolREff; // amount that foliar resp. is shutdown if soil is frozen (0 = full shutdown, 1 = no shutdown)
  double frozenSoilThreshold; // soil temperature below which frozenSoilFolREff and frozenSoilEff kick in (degrees C)

  // soil respiration:
  double litterBreakdownRate; /* rate at which litter is converted to soil/respired
								 at 0 degrees C and max soil moisture
								 (g C broken down * g^-1 litter C * day^-1)
								 NOTE: read in as per-year rate */
  double fracLitterRespired; /* of the litter broken down, fraction respired
								(the rest is transferred to soil pool) */
  double baseSoilResp; /* soil respiration at 0 degrees C and max soil moisture 
			 (g C respired * g^-1 soil C * day^-1) 
			 NOTE: read in as per-year rate! */
  double baseSoilRespCold; /* soil respiration at 0 degrees C and max soil moisture when tsoil < coldSoilThreshold
			      (g C respired * g^-1 soil C * day^-1)
			      NOTE: read in as per-year rate! */
  double soilRespQ10; // scalar determining effect of temp on soil resp.
  double soilRespQ10Cold; // scalar determining effect of temp on soil resp. when tsoil < coldSoilThreshold
  double coldSoilThreshold; // temp. at which use baseSoilRespCold and soilRespQ10Cold (if SEASONAL_R_SOIL true) (degrees C)
  double E0; // E0 in Lloyd-Taylor soil respiration function
  double T0; // T0 in Lloyd-Taylor soil respiration function
  double soilRespMoistEffect; // scalar determining effect of moisture on soil resp.

  // moisture-related:
  double waterRemoveFrac; /* fraction of plant available soil water which can be removed in one day
			    without water stress occurring */
  double frozenSoilEff; /* fraction of water that's available if soil is frozen (0 = none available, 1 = all still avail.)
			   NOTE: if frozenSoilEff = 0, then shut down psn. even if WATER_PSN = 0, if soil is frozen 
			   (if frozenSoilEff > 0, it has no effect if WATER_PSN = 0) */
  double wueConst; // water use efficiency constant
  double litterWHC; // litter (evaporative layer) water holding capacity (cm)
  double soilWHC; // soil (transpiration layer) water holding capacity (cm)
  double immedEvapFrac; // fraction of rain that is immediately intercepted & evaporated
  double fastFlowFrac; // fraction of water entering soil that goes directly to drainage
  double snowMelt; // rate at which snow melts (cm water equiv./degree C/day)
  double litWaterDrainRate; // rate at which litter water drains into lower layer when litter layer fully moisture-saturated (cm water/day)
  double rdConst; // scalar determining amount of aerodynamic resistance
  double rSoilConst1, rSoilConst2; /* soil resistance = e^(rSoilConst1 - rSoilConst2 * W1)
				      where W1 = (litterWater/litterWHC) */

  double leafCSpWt; // g C * m^-2 leaf area
  double cFracLeaf; // g leaf C * g^-1 leaf

  double turnoverRate; /* average turnover rate of woody plant C, in fraction per day
			  (leaf loss handled separately)
			  NOTE: read in as per-year rate! */

  // calculated parameters:
  double psnTMax; // degrees C - assumed symmetrical around psnTOpt
} Params;


#define NUM_PARAMS 54 // this only counts parameters read from file



// the state of the environment
typedef struct Environment {
  double plantWoodC; // carbon in plant wood (above-ground + roots) (g C * m^-2 ground area)
  double plantLeafC; // carbon in leaves (g C * m^-2 ground area)
  double litter; // carbon in litter (g C * m^-2 ground area)
  double soil; // carbon in soil (g C * m^-2 ground area)
  double litterWater; // water in litter (evaporative) layer (cm)
  double soilWater; // plant available soil water (cm)
  double snow; // snow pack (cm water equiv.)
} Envi;

// fluxes as per-day rates
typedef struct FluxVars {
  double photosynthesis; // GROSS photosynthesis (g C * m^-2 ground area * day^-1)
  double leafCreation; // g C * m^-2 ground area * day^-1 transferred from wood to leaves
  double leafLitter; // g C * m^-2 ground area * day^-1 leaves falling
  double woodLitter; // g C * m^-2 ground area * day^-1
  double rVeg; // vegetation respiration (g C * m^-2 ground area * day^-1)
  double litterToSoil; // g C * m^-2 ground area * day^-1 litter turned into soil
  double rLitter; // g C * m^-2 ground area * day^-1 respired by litter
  double rSoil; // g C * m^-2 ground area * day^-1 respired by soil
  double rain; // cm water * day^-1 (only liquid precip.)
  double snowFall; // cm water equiv. * day^-1
  double immedEvap; // rain that's intercepted and immediately evaporated (cm water * day^-1)
  double snowMelt; // cm water equiv. * day^-1
  double sublimation; // cm water equiv. * day^-1
  double fastFlow; // water entering soil that goes directly to drainage (out of system) (cm water * day^-1)
  double evaporation; // evaporation from top of soil (cm water * day^-1)
  double topDrainage; // drainage from top of soil to lower level (cm water * day^-1)
  double bottomDrainage; // drainage from lower level of soil out of system (cm water * day^-1)
  double transpiration; // cm water * day^-1
} Fluxes;


typedef struct TrackerVars { // variables to track various things
  double gpp; // g C * m^-2 taken up in this time interval: GROSS photosynthesis
  double rtot; // g C * m^-2 respired in this time interval
  double ra; // g C * m^-2 autotrophic resp. in this time interval
  double rh; // g C * m^-2 heterotrophic resp. in this time interval
  double npp; // g C * m^-2 taken up in this time interval
  double nee; // g C * m^-2 given off in this time interval
  double yearlyGpp; // g C * m^-2 taken up, year to date: GROSS photosynthesis
  double yearlyRtot; // g C * m^-2 respired, year to date
  double yearlyRa; // g C * m^-2 autotrophic resp., year to date
  double yearlyRh; // g C * m^-2 heterotrophic resp., year to date
  double yearlyNpp; // g C * m^-2 taken up, year to date
  double yearlyNee; // g C * m^-2 given off, year to date
  double totGpp; // g C * m^-2 taken up, to date: GROSS photosynthesis
  double totRtot; // g C * m^-2 respired, to date
  double totRa; // g C * m^-2 autotrophic resp., to date
  double totRh; // g C * m^-2 heterotrophic resp., to date
  double totNpp; // g C * m^-2 taken up, to date
  double totNee; // g C * m^-2 given off, to date
  double evapotranspiration; // cm water evaporated/sublimated (sublimed???) or transpired in this time step
  double soilWetnessFrac; /* mean fractional soil wetness (soilWater/soilWHC) over this time step
			     (linear mean: mean of wetness at start of time step and wetness at end of time step) */
} Trackers;


typedef struct PhenologyTrackersStruct { /* variables to track each year's phenological development
				  Only used in leafFluxes function, but made global so can be
				  initialized dynamically, based on day of year of first climate record */
  int didLeafGrowth; // have we done leaf growth at start of growing season this year? (0 = no, 1 = yes)
  int didLeafFall; // have we done leaf fall at end of growing season this year? (0 = no, 1 = yes)
  int lastYear; // year of previous time step, for tracking when we hit a new calendar year
} PhenologyTrackers;

// global variables:
// made global so they can be initialized in a separate function
// so they only have to be initialized once

static ClimateNode **firstClimates; // a vector of pointers to first climates of each point in space

// more global variables:
// these are global to increase efficiency (avoid lots of parameter passing)

static Params params;
static Envi envi; // state variables
static Trackers trackers;
static PhenologyTrackers phenologyTrackers; 
static MeanTracker *meanNPP; // running mean of NPP over some fixed time (stored in g C * m^-2 * day^-1)
static ClimateNode *climate; // current climate
static Fluxes fluxes;
static double *outputPtrs[MAX_DATA_TYPES]; // pointers to different possible outputs


/* Read climate file into linked lists, 
   make firstClimates be a vector where each element is a pointer to the head of a list corresponding to one spatial location

   return an array containing the number of time steps in each location (dynamically allocated with malloc)

   numLocs = number of spatial locations: there should be one set of entries in climFile for each location,
   or a location can be skipped, and we'll use climate at location 0 for that location, too

   format of climFile:
   each line represents one time step, with the following format:
   location year day time intervalLength tair tsoil par precip vpd vpdSoil vPress wspd soilWetness
   Note: there should be NO blank lines in file, even between different spatial locations;
   all entries for a given location should be contiguous, and locations should be in ascending order
   There may be locations for which there is no climate data, in which case we use climate from location 0
   If intervalLength is negative, then it is interpreted as being in seconds rather than days (e.g. -1800 for 1/2 hour time steps)
   - In this case, it is multiplied by -1 and converted to days before being stored in the climate linked list
*/
int * readClimData(char *climFile, int numLocs) {
  FILE *in;
  ClimateNode *curr, *next;
  int loc, year, day;
  int lastYear;
  double time, length; // time in hours, length in days (or fraction of day)
  double tair, tsoil, par, precip, vpd, vpdSoil, vPress, wspd, soilWetness;
  int currLoc;
  int count;
  int i;
  int *steps; // # of time steps in each location

#if GDD
  double thisGdd; // growing degree days of this time step
  double gdd = 0.0; // growing degree days since the last Jan. 1
#endif

  int status; // status of the read

  steps = (int *)malloc(numLocs * sizeof(int));
  for (i = 0; i < numLocs; i++)
    steps[i] = 0; // 0 will denote nothing read

  in = openFile(climFile, "r");
 
  status = fscanf(in, "%d %d %d %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf", &loc, &year, &day, &time, &length, &tair, &tsoil, &par, &precip, &vpd, &vpdSoil, &vPress, &wspd, &soilWetness);
  if (status == EOF) {
    printf("Error: no climate data in %s\n", climFile);
    exit(1);
  }

  if (loc != 0) {
    printf("Error reading from climate file: first location must be loc. 0\n");
    exit(1);
  }
  
  firstClimates = (ClimateNode **)malloc(numLocs * sizeof(ClimateNode *)); // vector of heads
  for (currLoc = 0; currLoc < numLocs; currLoc++)
    firstClimates[currLoc] = NULL; // default is null
  // if firstClimates[i] stays null for some i, that means we didn't read any climate data for location i, so use climate from location 0
  
  currLoc = loc;
  firstClimates[currLoc] = (ClimateNode *)malloc(sizeof(ClimateNode)); // allocate space for head
  curr = next = firstClimates[currLoc];
  count = 0;
  while (status != EOF) {
    // we have another day's climate
    curr = next;
    count++; // # of time steps in this location
    
    curr->year = year;
    curr->day = day;
    curr->time = time;

    if (length < 0) // parse as seconds
      length = length/-86400.; // convert to days
    curr->length = length;

    curr->tair = tair;
    curr->tsoil = tsoil;
    curr->par = par * (1.0/length);
    // convert par from Einsteins * m^-2 to Eisteins * m^-2 * day^-1
    curr->precip = precip * 0.1; // convert from mm to cm
    curr->vpd = vpd * 0.001; // convert from Pa to kPa
    if (curr->vpd < TINY)
      curr->vpd = TINY; // avoid divide by zero
    curr->vpdSoil = vpdSoil * 0.001; // convert from Pa to kPa
    curr->vPress = vPress * 0.001; // convert from Pa to kPa
    curr->wspd = wspd;
    if (curr->wspd < TINY)
      curr->wspd = TINY; // avoid divide by zero
    curr->soilWetness = soilWetness;

#if GDD
    if (year != lastYear) // HAPPY NEW YEAR!
      gdd = 0; // reset growing degree days
    thisGdd = tair * length; 
    if (thisGdd < 0) // can't have negative growing degree days
      thisGdd = 0;
    gdd += thisGdd;
    curr->gdd = gdd;
#endif

    lastYear = year;

    status = fscanf(in, "%d %d %d %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf", &loc, &year, &day, &time, &length, &tair, &tsoil, &par, &precip, &vpd, &vpdSoil, &vPress, &wspd, &soilWetness);

    if (status != EOF) { // we have another climate record
      // check new location, compare with old location (currLoc), make sure new location is valid, and act accordingly:
      if (loc == currLoc) { // still reading climate records from the same place: add a node at end of linked list
	next = (ClimateNode *)malloc(sizeof(ClimateNode));
	curr->nextClim = next;
	// set this down here rather than at top of loop so head treated the same as rest of list
      }
      else if (loc >= numLocs) { // (loc == numLocs is an error since we use 0-indexing)
	printf("Error reading climate file: trying to read location %d, but numLocs = %d\n", loc, numLocs);
	exit(1);
      }
      else if (loc > currLoc) { // we've advanced to the next location (note: possible that we skipped some locations: this is OK)
	curr->nextClim = NULL; // terminate last linked list
	steps[currLoc] = count; // record the number of time steps for last location
	count = 0; // reset count of # of time steps
#if GDD
	gdd = 0; // reset growing degree days
#endif
	currLoc = loc; 
	firstClimates[currLoc] = (ClimateNode *)malloc(sizeof(ClimateNode)); // allocate space for head
	curr = next = firstClimates[currLoc]; // we'll start writing to next linked list 
      }
      else { // loc < currLoc
	printf("Error reading climate file: was reading location %d, trying to read location %d\n", currLoc, loc);
	printf("Climate records for a given location should be contiguous, and locations should be in ascending order\n");
	exit(1);
      }
    }
    else { // status == EOF - no more records
      curr->nextClim = NULL; // terminate this last linked list
      steps[currLoc] = count; // record the number of time steps for last location
    }

  } // end while

  fclose(in);

  for (i = 0; i < numLocs; i++)
    if (steps[i] == 0) // nothing read for this location
      steps[i] = steps[0]; // this location will duplicate location 0

  return steps;
}

// allocate & initialize spatialParamsPtr (a pointer to a SpatialParams pointer to allow for allocation)
// read in parameter file, put parameters (and other info) in spatialParams 
// return numLocs (read from first line of spatialParams file)
int readParamData(SpatialParams **spatialParamsPtr, char *paramFile, char *spatialParamFile) {
  FILE *paramF, *spatialParamF;
  SpatialParams *spatialParams; // a local variable to prevent lots of unnecessary dereferences
  int numLocs;

  paramF = openFile(paramFile, "r");
  spatialParamF = openFile(spatialParamFile, "r");

  fscanf(spatialParamF, "%d", &numLocs);

  *spatialParamsPtr = newSpatialParams(NUM_PARAMS, numLocs);
  spatialParams = *spatialParamsPtr; // to prevent lots of unnecessary dereferences

  readOneSpatialParam(spatialParams, paramF, spatialParamF, "plantWoodInit", &(params.plantWoodInit));
  readOneSpatialParam(spatialParams, paramF, spatialParamF, "laiInit", &(params.laiInit));
  readOneSpatialParam(spatialParams, paramF, spatialParamF, "litterInit", &(params.litterInit));
  readOneSpatialParam(spatialParams, paramF, spatialParamF, "soilInit", &(params.soilInit));
  readOneSpatialParam(spatialParams, paramF, spatialParamF, "litterWFracInit", &(params.litterWFracInit));
  readOneSpatialParam(spatialParams, paramF, spatialParamF, "soilWFracInit", &(params.soilWFracInit));
  readOneSpatialParam(spatialParams, paramF, spatialParamF, "snowInit", &(params.snowInit));

  readOneSpatialParam(spatialParams, paramF, spatialParamF, "aMax", &(params.aMax));
  readOneSpatialParam(spatialParams, paramF, spatialParamF, "aMaxFrac", &(params.aMaxFrac));
  readOneSpatialParam(spatialParams, paramF, spatialParamF, "baseFolRespFrac", &(params.baseFolRespFrac));
  readOneSpatialParam(spatialParams, paramF, spatialParamF, "psnTMin", &(params.psnTMin));
  readOneSpatialParam(spatialParams, paramF, spatialParamF, "psnTOpt", &(params.psnTOpt));
  readOneSpatialParam(spatialParams, paramF, spatialParamF, "vegRespQ10", &(params.vegRespQ10));
  readOneSpatialParam(spatialParams, paramF, spatialParamF, "growthRespFrac", &(params.growthRespFrac));
  readOneSpatialParam(spatialParams, paramF, spatialParamF, "frozenSoilFolREff", &(params.frozenSoilFolREff));
  readOneSpatialParam(spatialParams, paramF, spatialParamF, "frozenSoilThreshold", &(params.frozenSoilThreshold));
  readOneSpatialParam(spatialParams, paramF, spatialParamF, "dVpdSlope", &(params.dVpdSlope));
  readOneSpatialParam(spatialParams, paramF, spatialParamF, "dVpdExp", &(params.dVpdExp));
  readOneSpatialParam(spatialParams, paramF, spatialParamF, "halfSatPar", &(params.halfSatPar));
  readOneSpatialParam(spatialParams, paramF, spatialParamF, "attenuation", &(params.attenuation));
  readOneSpatialParam(spatialParams, paramF, spatialParamF, "leafOnDay", &(params.leafOnDay));
  readOneSpatialParam(spatialParams, paramF, spatialParamF, "gddLeafOn", &(params.gddLeafOn));
  readOneSpatialParam(spatialParams, paramF, spatialParamF, "soilTempLeafOn", &(params.soilTempLeafOn));
  readOneSpatialParam(spatialParams, paramF, spatialParamF, "leafOffDay", &(params.leafOffDay));
  readOneSpatialParam(spatialParams, paramF, spatialParamF, "leafGrowth", &(params.leafGrowth));
  readOneSpatialParam(spatialParams, paramF, spatialParamF, "fracLeafFall", &(params.fracLeafFall));
  readOneSpatialParam(spatialParams, paramF, spatialParamF, "leafAllocation", &(params.leafAllocation));
  readOneSpatialParam(spatialParams, paramF, spatialParamF, "leafTurnoverRate", &(params.leafTurnoverRate));
  readOneSpatialParam(spatialParams, paramF, spatialParamF, "baseVegResp", &(params.baseVegResp));
  readOneSpatialParam(spatialParams, paramF, spatialParamF, "litterBreakdownRate", &(params.litterBreakdownRate));
  readOneSpatialParam(spatialParams, paramF, spatialParamF, "fracLitterRespired", &(params.fracLitterRespired));
  readOneSpatialParam(spatialParams, paramF, spatialParamF, "baseSoilResp", &(params.baseSoilResp));
  readOneSpatialParam(spatialParams, paramF, spatialParamF, "baseSoilRespCold", &(params.baseSoilRespCold));
  readOneSpatialParam(spatialParams, paramF, spatialParamF, "soilRespQ10", &(params.soilRespQ10));
  readOneSpatialParam(spatialParams, paramF, spatialParamF, "soilRespQ10Cold", &(params.soilRespQ10Cold));
  readOneSpatialParam(spatialParams, paramF, spatialParamF, "coldSoilThreshold", &(params.coldSoilThreshold));
  readOneSpatialParam(spatialParams, paramF, spatialParamF, "E0", &(params.E0));
  readOneSpatialParam(spatialParams, paramF, spatialParamF, "T0", &(params.T0));
  readOneSpatialParam(spatialParams, paramF, spatialParamF, "soilRespMoistEffect", &(params.soilRespMoistEffect));
  readOneSpatialParam(spatialParams, paramF, spatialParamF, "waterRemoveFrac", &(params.waterRemoveFrac));
  readOneSpatialParam(spatialParams, paramF, spatialParamF, "frozenSoilEff", &(params.frozenSoilEff));
  readOneSpatialParam(spatialParams, paramF, spatialParamF, "wueConst", &(params.wueConst));
  readOneSpatialParam(spatialParams, paramF, spatialParamF, "litterWHC", &(params.litterWHC));
  readOneSpatialParam(spatialParams, paramF, spatialParamF, "soilWHC", &(params.soilWHC));
  readOneSpatialParam(spatialParams, paramF, spatialParamF, "immedEvapFrac", &(params.immedEvapFrac));
  readOneSpatialParam(spatialParams, paramF, spatialParamF, "fastFlowFrac", &(params.fastFlowFrac));
  readOneSpatialParam(spatialParams, paramF, spatialParamF, "snowMelt", &(params.snowMelt));
  readOneSpatialParam(spatialParams, paramF, spatialParamF, "litWaterDrainRate", &(params.litWaterDrainRate));
  readOneSpatialParam(spatialParams, paramF, spatialParamF, "rdConst", &(params.rdConst));
  readOneSpatialParam(spatialParams, paramF, spatialParamF, "rSoilConst1", &(params.rSoilConst1));
  readOneSpatialParam(spatialParams, paramF, spatialParamF, "rSoilConst2", &(params.rSoilConst2));
  readOneSpatialParam(spatialParams, paramF, spatialParamF, "leafCSpWt", &(params.leafCSpWt));
  readOneSpatialParam(spatialParams, paramF, spatialParamF, "cFracLeaf", &(params.cFracLeaf));
  readOneSpatialParam(spatialParams, paramF, spatialParamF, "turnoverRate", &(params.turnoverRate));

  fclose(paramF);
  fclose(spatialParamF);

  if (!(allParamsRead(spatialParams))) {
    printf("Error: Didn't read as many parameters as expected\n");
    exit(1);
  }

  return numLocs;
}


// pre: out is open for writing
// print header line to output file
void outputHeader(FILE *out) {
  fprintf(out, "(PlantWoodC, PlantLeafC, Soil and Litter in g C/m^2; Water and Snow in cm; SoilWetness is fraction of WHC;\n");
  fprintf(out, " NPP, YearlyNPP, NEE, TotNEE and SoilResp in g C/m^2 [positive NPP = UPTAKE, positive NEE and SoilResp = LOSS]\n");
  fprintf(out, " Evapotranspiration in cm H2O over whole time step)\n");
  fprintf(out, "Location Year Day  Time PlantWoodC PlantLeafC Soil   Litter LitterWater SoilWater SoilWetness Snow   NPP YearlyNPP     NEE   TotNEE Evapotranspiration\n");
}

// pre: out is open for writing
// print current state to output file
void outputState(FILE *out, int loc, int year, int day, double time) {
  fprintf(out, "%8d %4d %3d %5.2f %8.2f %8.2f %8.2f %8.2f %8.2f %8.2f %8.3f %8.2f %8.2f %8.2f %8.8f %8.2f %8.3f\n", 
	  loc, year, day, time,
	  envi.plantWoodC, envi.plantLeafC, envi.soil, envi.litter, envi.litterWater, envi.soilWater, trackers.soilWetnessFrac, envi.snow,
	  trackers.npp, trackers.yearlyNpp, trackers.nee, trackers.totNee, trackers.evapotranspiration);
}


// de-allocate space used for climate linked list
void freeClimateList(int numLocs) {
  ClimateNode *curr, *prev;
  int loc;

  for (loc = 0; loc < numLocs; loc++) { // loop through firstClimates, deallocating each linked list
    curr = firstClimates[loc];
    while (curr != NULL) {
      prev = curr;
      curr = curr->nextClim;
      free(prev);
    }
  }
  
  // and finally deallocate the vector itself:
  free(firstClimates);
}


// !!! functions for calculating auxiliary variables !!! 
// rather than returning a value, they have as parameters the variable(s) which they modify
// so a single function can modify multiple variables


// calculate amount of light absorbed
// using light attenuation (as in PnET)
// CURRENTLY UNUSED (see calcLightEff3)
void calcLightEff2 (double *lightEff, double lai, double par) {
  static const int NUM_LAYERS = 50;

  int layer; // counter
  double cumLai = 0.0; // lai from this layer up
  double lightIntensity;
  double cumLightEff = 0.0;
  
  if (lai > 0 && par > 0) { // must have at least some leaves and some light; DANGER: sensitive to round-off errors: e.g. lai = -0.00000001 will give a very different lightEff from lai = 0.000000001; but since we later multiply by lai (in potPsn), this shouldn't lead to any major problems
    for (layer = 1; layer <= NUM_LAYERS; layer++) {
      cumLai = lai * ((double)layer / NUM_LAYERS); // lai from this layer up
      lightIntensity = par * exp(-1.0 * params.attenuation * cumLai); // between 0 and par
      cumLightEff += (1 - pow(2, (-1.0 * lightIntensity/params.halfSatPar))); // add between 0 and 1
      // when lightIntensity = halfSatPar, add 1/2
    }
    
    *lightEff = cumLightEff/(double)NUM_LAYERS; // find average light effect
  }
  else // no leaves or no light!
    *lightEff = 0;
}


// another method to calculate amount of light absorbed
// using light attenuation (as in PnET)
// difference between this and calcLightEff2 is that here we use Simpson's method to approximate the integral
void calcLightEff3 (double *lightEff, double lai, double par) {
  /*
    We are essentially integrating over the canopy, from top to bottom,
    but it's an ugly integral, so we'll approximate it numerically.
    Here we use Simpson's method to approximate the integral, so we need an odd number of points.
    This means that NUM_LAYERS must be EVEN because we loop from layer = 0 to layer = NUM_LAYERS
 
    As a reminder, Simpson's rule approximates the integral as:
    (h/3) * (y(0) + 4y(1) + 2y(2) + 4y(3) + ... + 2y(n-2) + 4y(n-1) + y(n)),
    where h is the distance between each x value (here h = 1/NUM_LAYERS).
    We keep track of the running sum in cumLightEff.
  */

  static const int NUM_LAYERS = 6;
  // believe it or not, 6 layers gives approximately the same result as 100 layers

  int layer; // counter
  double cumLai; // lai from this layer up
  double lightIntensity;
  double currLightEff, cumLightEff;
  int coeff; // current coefficient in Simpson's rule
  
  if (lai > 0 && par > 0) { // must have at least some leaves and some light; DANGER: sensitive to round-off errors: e.g. lai = -0.00000001 will give a very different lightEff from lai = 0.000000001; but since we later multiply by lai (in potPsn), this shouldn't lead to any major problems
    cumLai = 0.0;
    cumLightEff = 0.0; // the running sum
    layer = 0;
    coeff = 1;

    while (layer <= NUM_LAYERS) {
      cumLai = lai * ((double)layer / NUM_LAYERS); // lai from this layer up
      lightIntensity = par * exp(-1.0 * params.attenuation * cumLai); // between 0 and par
      currLightEff = (1 - pow(2, (-1.0 * lightIntensity/params.halfSatPar))); // between 0 and 1
      // when lightIntensity = halfSatPar, currLightEff = 1/2
      cumLightEff += coeff * currLightEff;
    
      // now move to the next layer:
      layer++;
      coeff = 2*(1 + layer%2); // coeff. goes 1, 4, 2, 4, ..., 2, 4, 2
    }
    // last value should have had a coefficient of 1, but actually had a coefficient of 2,
    // so subtract 1:
    cumLightEff -= currLightEff;
    
    *lightEff = cumLightEff/(3.0*NUM_LAYERS); // multiplying by (h/3) in Simpson's rule
  }
  else // no leaves or no light!
    *lightEff = 0;
}



// calculate gross photosynthesis without water effect (g C * m^-2 ground area * day^-1)
// and base foliar respiration without temp, water, etc. (g C * m^-2 ground area * day^-1)
void potPsn(double *potGrossPsn, double *baseFolResp, double lai, double tair, double vpd, double par, int day) {
  double grossAMax; // maximum possible gross respiration (nmol CO2 * g^-1 leaf * sec^-1)
  double dTemp, dVpd, lightEff; // decrease in photosynth. due to temp, vpd and amt. of light absorbed
  double respPerGram; // base foliar respiration in nmol CO2 * g^-1 leaf * sec^-1
  double conversion; /* convert from (nmol CO2 * g^-1 leaf * sec^-1) 
		       to (g C * m^-2 ground area * day^-1) */

  respPerGram = params.baseFolRespFrac * params.aMax; 
  // foliar respiration, unmodified by temp, etc.
  grossAMax = params.aMax * params.aMaxFrac + respPerGram;
  
  dTemp = (params.psnTMax - tair)*(tair - params.psnTMin)/pow((params.psnTMax - params.psnTMin)/2.0, 2);
  if (dTemp < 0)
    dTemp = 0.0;
  
  dVpd = 1.0 - params.dVpdSlope * pow(vpd, params.dVpdExp);
  
  calcLightEff3(&lightEff, lai, par);
  
  conversion = C_WEIGHT * (1.0/TEN_9) * (params.leafCSpWt/params.cFracLeaf) * lai * SEC_PER_DAY; // to convert units
  *potGrossPsn = grossAMax * dTemp * dVpd * lightEff * conversion;
  *baseFolResp = respPerGram * conversion; // do foliar resp. even if no photosynthesis in this time step

  // printf("%f %f %f %f %f %f ", climate->length, grossAMax, conversion, dTemp, dVpd, lightEff);
}

// calculate transpiration (cm H20 * day^-1)
// and dWater (factor between 0 and 1)
void moisture(double *trans, double *dWater, double potGrossPsn, double vpd, double soilWater) {
  double potTrans; // potential transpiration in the absense of plant water stress (cm H20 * day^-1)
  double removableWater;
  double wue; // water use efficiency, in mg CO2 fixed * g^-1 H20 transpired

  if (potGrossPsn < TINY) { // avoid divide by 0
    *trans = 0.0; // no photosynthesis -> no transpiration
    *dWater = 1; // dWater doesn't matter, since we don't have any photosynthesis
  }

  else {
    wue = params.wueConst/vpd;
    potTrans = potGrossPsn/wue * 1000.0 * (44.0/12.0) * (1.0/10000.0); 
    // 1000 converts g to mg; 44/12 converts g C to g CO2, 1/10000 converts m^2 to cm^2

    removableWater = soilWater * params.waterRemoveFrac;
    if (climate->tsoil < params.frozenSoilThreshold) // frozen soil - less or no water available
      removableWater *= params.frozenSoilEff; /* frozen soil effect: fraction of water available if soil is frozen
						 (assume amt. of water avail. w/ frozen soil scales linearly with amt. of
						 water avail. in thawed soil) */
    if (removableWater >= potTrans)
      *trans = potTrans;
    else
      *trans = removableWater;


#if WATER_PSN // we're modeling water stress
    *dWater = *trans/potTrans; // from PnET: equivalent to setting DWATER_MAX = 1
#else // WATER_PSN = 0
    if (climate->tsoil < params.frozenSoilThreshold && params.frozenSoilEff == 0)
      // (note: can't have partial shutdown of psn with frozen soil if WATER_PSN = 0)
      *dWater = 0; // still allow total shut down of psn. if soil is frozen
    else // either soil is thawed, or frozenSoilEff > 0
      *dWater = 1; // no water stress, even if *trans/potTrans < 1
#endif // WATER_PSN
  }

  // printf("%f\n", *dWater);
}


// have we passed the growing season-start leaf growth trigger this year?
// 0 = no, 1 = yes
// note: there may be some fluctuations in this signal for some methods of determining growing season start
// (e.g. for soil temp-based leaf growth)
int pastLeafGrowth() {
#if GDD
  return (climate->gdd >= params.gddLeafOn); // growing degree days threshold
#elif SOIL_PHENOL
  return (climate->tsoil >= params.soilTempLeafOn); // soil temperature threshold
#else
  return ((climate->day + climate->time/24.0) >= params.leafOnDay); // turn-on day
#endif
}

// have we passed the growing season-end leaf fall trigger this year?
// 0 = no, 1 = yes
int pastLeafFall() {
  return ((climate->day + climate->time/24.0) >= params.leafOffDay); // turn-off day
}


// calculate leafCreation and leafLitter fluxes (g C/m^2 ground/day)
// leafCreation is a fraction of recent mean npp, plus some constant amount at start of growing season
// leafLitter is a constant rate, plus some additional fraction of leaves at end of growing season
void leafFluxes(double *leafCreation, double *leafLitter, double plantLeafC) {
  double npp; // temporal mean of recent npp (g C * m^-2 ground * day^-1)

  npp = getMeanTrackerMean(meanNPP);
  // first determine the fluxes that happen at every time step, not just start & end of growing season:
  if (npp > 0)
    *leafCreation = npp * params.leafAllocation; // a fraction of NPP is allocated to leaf growth
  else // net loss of C in this time step - no C left for growth
    *leafCreation = 0;
  *leafLitter = plantLeafC * params.leafTurnoverRate; // a constant fraction of leaves fall in each time step  

  // now add add'l fluxes at start/end of growing season:

  // first check for new year; if new year, reset trackers (since we haven't done leaf growth or fall yet in this new year):
  if (climate->year > phenologyTrackers.lastYear) { // HAPPY NEW YEAR!
    phenologyTrackers.didLeafGrowth = 0;
    phenologyTrackers.didLeafFall = 0;
    phenologyTrackers.lastYear = climate->year;
  }

  // check for start of growing season:
  if (!phenologyTrackers.didLeafGrowth && pastLeafGrowth()) { // we just reached the start of the growing season
    *leafCreation += (params.leafGrowth / climate->length);
    phenologyTrackers.didLeafGrowth = 1;
  }

  // check for end of growing season:
  if (!phenologyTrackers.didLeafFall && pastLeafFall()) { // we just reached the end of the growing season
    *leafLitter += (plantLeafC * params.fracLeafFall) / climate->length;
    phenologyTrackers.didLeafFall = 1;
  }
}


// calculate rain and snowfall (cm water equiv./day)
// calculate snow melt (cm water equiv./day) and drainage(cm/day)
// drainage here is any water that exceeds water holding capacity

// this is the simplified water flow function, which has only one soil moisture layer
// and does not do evaporation of any kind, or fast flow (sets these all to 0)
void simpleWaterFlow(double *rain, double *snowFall, double *immedEvap, double *snowMelt, double *sublimation,
		     double *fastFlow, double *evaporation, double *topDrainage, double *bottomDrainage, 
		     double water, double snow, double precip, double temp, double length, double trans)
{
  double netIn; // net water into soil, in cm

#if SNOW // we're modeling snow
  if (temp <= 0) { // below freezing
	*snowFall = precip/length;
	*rain = 0;
	*snowMelt = 0;
  }
  else { // above freezing 
	*snowFall = 0;
	*rain = precip/length;
	if (snow > 0) {
	  *snowMelt = params.snowMelt * temp; // snow melt proportional to temp.
	  if ((*snowMelt * length) > snow) // can only melt what's there!
	    *snowMelt = snow/length;
	}
	else
	  *snowMelt = 0;
  }

#else // not modeling snow
  *rain = precip/length;
  *snowFall = 0;
  *snowMelt = 0;
#endif // #if snow

  netIn = (*rain + *snowMelt - trans) * length;
  *bottomDrainage = ((water + netIn) - params.soilWHC)/length;
  if (*bottomDrainage < 0)
    *bottomDrainage = 0;

  // all the things we don't model in simpleWaterFlow mode:
  *immedEvap = *sublimation = *fastFlow = *evaporation = *topDrainage = 0;
}


// following 4 functions are for complex water sub-model

// calculate total rain and snowfall (cm water equiv./day)
// also, immediate evaporation (from interception) (cm/day)
void calcPrecip(double *rain, double *snowFall, double *immedEvap)
{
  // below freezing -> precip falls as snow
  if (climate->tair <= 0) {
    *snowFall = climate->precip/climate->length;
    *rain = 0;
  }

  // above freezing -> precip falls as rain
  else {
    *snowFall = 0;
    *rain = climate->precip/climate->length;
  }

  /* Immediate evaporation is a sum of evaporation from canopy interception
     and evaporation from pools on the ground
     Higher LAI will mean more canopy evap. but less evap. from pools on the ground
     For now we'll assume that these two effects cancel, and immediate evap. is a constant fraction
     Note that we don't evaporate snow here (we'll let sublimation take care of that)
  */
  *immedEvap = (*rain) * params.immedEvapFrac;
}

// snowpack dynamics:
// calculate snow melt (cm water equiv./day) & sublimation (cm water equiv./day)
// ensure we don't overdrain the snowpack (so that it becomes negative)
// snowFall in cm/day
void snowPack(double *snowMelt, double *sublimation, double snowFall) 
{
  // conversion factor for sublimation
  static const double CONVERSION = (RHO * CP)/GAMMA * (1./LAMBDA_S) 
    * 1000. * 1000. * (1./10000) * SEC_PER_DAY;
  // 1000 converts kg to g, 1000 converts kPa to Pa, 1/10000 converts m^2 to cm^2

  double rd; // aerodynamic resistance between ground and canopy air space (sec/m)
  double snowRemaining; // to make sure we don't get rid of more than there is

  /* If no snow, set fluxes to 0
     Note that this formulation means that there could be big differences in the fluxes depending on whether the snow pack is 0 or slightly greater than 0
     e.g. if there is snow fall in timestep i but no snow pack, then there will be no sublimation or snowMelt, but if there is a small snow pack, then there can be sublimation
     This could have the unrealistic result that, with no snow pack in timestep i, there is actually a larger snow pack in timestep (i+1) than there would be if there had been a snow pack in timestep i
     However, if we allow sublimation when there is snowfall but no snow pack, then we could have sublimation and evaporation from the soil in the same time step, which may be even worse
     Note that there is, in principle, no reason why we should set snowMelt = 0 if there is snow fall but no snow pack,
     in practice (the way the model is written now), snowMelt will always = 0 in this case anyway, since snow fall only happens when tair < 0 and snow melt only when tair > 0
     Note that if we wanted to allow sublimation when there is snow fall but no snow pack, then the check here could be rewritten as if (snowRemaining <= 0) rather than envi.snow <= 0 (and snowRemaining could be calculated before the check)
  */
  if (envi.snow <= 0) {
    *snowMelt = 0;
    *sublimation = 0;
  }
  
  // else: there is snow
  else {
    // first calculate sublimation, then snow melt
    // (if there's not enough snow to do both, priority given to sublimation)
    rd = (params.rdConst)/(climate->wspd); // aerodynamic resistance (sec/m)
    *sublimation = CONVERSION * (E_STAR_SNOW - climate->vPress)/rd;

    snowRemaining = envi.snow + (snowFall * climate->length);
 
    // remove to allow sublimation of a negative amount of snow
    // right now we can't sublime a negative amount of snow
    if (*sublimation < 0)
      *sublimation = 0;

    // make sure we don't sublime more than there is to sublime:
    if (snowRemaining - (*sublimation * climate->length) < 0) {
      *sublimation = snowRemaining/climate->length;
      snowRemaining = 0;
    }
    
    else
      snowRemaining -= (*sublimation * climate->length);


    // below freezing: no snow melt
    if (climate->tair <= 0)
      *snowMelt = 0;

    // above freezing: melt snow
    else {      
      *snowMelt = params.snowMelt * climate->tair; // snow melt proportional to temp.

      // make sure we don't melt more than there is to melt:
      if (snowRemaining - (*snowMelt * climate->length) < 0)
	*snowMelt = snowRemaining/climate->length;
    } // end else above freezing
  } // end else there is snow

  // printf("%d %f %f %f %f %f %f\n", climate->day, climate->tair, climate->precip, envi.snow, *snowMelt, *sublimation, snowFall);
} // end snowPack
      

// water calculations relating to the top (litter/evaporative) layer of soil
// (if only modeling one soil water layer, these relate to that layer)
// calculates fastFlow (cm/day), evaporation (cm/day) and drainage to lower layer (cm/day)
// water is amount of water in evaporative layer (cm)
// whc is water holding capacity of evaporative layer (cm)
// (water and whc are used rather than envi variables to allow use of either litterWater or soilWater)
// net rain (cm/day) is (rain - immedEvap) - i.e. the amount available to enter the soil
// snowMelt in cm water equiv./day
// fluxesOut is the sum of any fluxes out of this layer that have already been calculated 
// (e.g. transpiration if we're just using one layer) (for calculating remaining water/drainage) (cm/day)
void evapSoilFluxes(double *fastFlow, double *evaporation, double *drainage,
	      double water, double whc, double netRain, double snowMelt, double fluxesOut)
{
  // conversion factor for evaporation
  static const double CONVERSION = (RHO * CP)/GAMMA * (1./LAMBDA) 
    * 1000. * 1000. * (1./10000) * SEC_PER_DAY;
  // 1000 converts kg to g, 1000 converts kPa to Pa, 1/10000 converts m^2 to cm^2

  double waterRemaining; /* keep running total of water remaining, in cm
			    (to make sure we don't evap. or drain too much, and so we can drain any overflow) */
  double netIn; // keep track of net water into soil, in cm/day
  double rd; // aerodynamic resistance between ground and canopy air space (sec/m)
  double rsoil; // bare soil surface resistance (sec/m)
  
  netIn = netRain + snowMelt;
  
  // fast flow: fraction that goes directly to drainage
  *fastFlow = netIn * params.fastFlowFrac;
  netIn -= *fastFlow;
  
  // calculate evaporation:
  // first calculate how much water is left to evaporate (used later)
  waterRemaining = water + netIn * climate->length - fluxesOut * climate->length;

  // if there's a snow pack, don't evaporate from soil:
  if (envi.snow > 0)
    *evaporation = 0;

  // else no snow pack:
  else {
    rd = (params.rdConst)/(climate->wspd); // aerodynamic resistance (sec/m)
    rsoil = exp(params.rSoilConst1 - params.rSoilConst2 * (water/whc));
    *evaporation = CONVERSION * climate->vpdSoil/(rd + rsoil);
    // by using vpd we assume that relative humidity of soil pore space is 1
    // (when this isn't true, there won't be much water evaporated anyway)

    // remove to allow negative evaporation (i.e. condensation):
    if (*evaporation < 0)
      *evaporation = 0;

    // make sure we don't evaporate more than we have:
    if (waterRemaining - (*evaporation * climate->length) < 0) {
      *evaporation = waterRemaining/climate->length; 
      waterRemaining = 0;
    }
    else
      waterRemaining -= (*evaporation * climate->length);
  }

#if LITTER_WATER_DRAINAGE // we're calculating drainage even when evap. layer is not overflowing
  *drainage = params.litWaterDrainRate * (water/whc); // drainage rate is proportional to fractional soil moisture
  // make sure we don't drain more than we have:
  if (waterRemaining - (*drainage * climate->length) < 0) {
    *drainage = waterRemaining/climate->length; 
    waterRemaining = 0;
  }
  else
    waterRemaining -= (*drainage * climate->length);
#else // LITTER_WATER_DRAINAGE = 0
  *drainage = 0;
#endif

  // drain any water that remains beyond water holding capacity:
  if (waterRemaining > whc)
    *drainage += (waterRemaining - whc)/(climate->length);
}


// calculate drainage from bottom (soil/transpiration) layer (cm/day)
// based on current soil water store (cm), whc, drainage from top (cm/day) and transpiration (cm/day)
void transSoilDrainage(double *bottomDrainage, double topDrainage, double trans, double soilWater) {
  double waterRemaining; // cm

  waterRemaining = soilWater + (topDrainage - trans) * climate->length;
  *bottomDrainage = (waterRemaining - params.soilWHC)/(climate->length);
  if (*bottomDrainage < 0)
    *bottomDrainage = 0;
}


// calculates fastFlow (cm/day), evaporation (cm/day) and drainage to lower layer (cm/day)
// net rain (cm/day) is (rain - immedEvap) - i.e. the amount available to enter the soil
// snowMelt in cm water equiv./day
// litterWater and soilWater in cm
// Also calculates drainage from bottom (soil/transpiration) layer (cm/day)
// Note that there may only be one layer, in which case we have only the bottomDrainage term,
// and evap. and trans. come from same layer.
void soilWaterFluxes(double *fastFlow, double *evaporation, double *topDrainage, double *bottomDrainage,
		     double netRain, double snowMelt, double trans, double litterWater, double soilWater) {

#if LITTER_WATER
  evapSoilFluxes(fastFlow, evaporation, topDrainage, litterWater, params.litterWHC, netRain, snowMelt, 0);
  // last parameter = fluxes out that have already been calculated = 0
  transSoilDrainage(bottomDrainage, *topDrainage, trans, soilWater);
#else // only one soil moisture pool: evap. and trans. both happen from this pool
  *topDrainage = 0; // no top layer, only a bottom layer
  evapSoilFluxes(fastFlow, evaporation, bottomDrainage, soilWater, params.soilWHC, netRain, snowMelt, trans);
  // last parameter = fluxes out that have already been calculated: transpiration
#endif

}


// calculate GROSS phtosynthesis (g C * m^-2 * day^-1)
void getGpp(double *gpp, double potGrossPsn, double dWater) {
  *gpp = potGrossPsn * dWater;
}


// calculate foliar respiration and wood maint. resp, both in g C * m^-2 ground area * day^-1
// does *not* explicitly model growth resp. (includes it in maint. resp)
void vegResp(double *folResp, double *woodResp, double baseFolResp) {
  *folResp = baseFolResp * pow(params.vegRespQ10, (climate->tair - params.psnTOpt)/10.0);
  if (climate->tsoil < params.frozenSoilThreshold)
    *folResp *= params.frozenSoilFolREff; // allows foliar resp. to be shutdown by a given fraction in winter

  *woodResp = params.baseVegResp * envi.plantWoodC * pow(params.vegRespQ10, climate->tair/10.0);
}

// a second veg. resp. method:
// calculate foliar resp., wood maint. resp. and growth resp., all in g C * m^-2 ground area * day^-1
// growth resp. modeled in a very simple way
void vegResp2(double *folResp, double *woodResp, double *growthResp, double baseFolResp, double gpp) {
  *folResp = baseFolResp * pow(params.vegRespQ10, (climate->tair - params.psnTOpt)/10.0);
  if (climate->tsoil < params.frozenSoilThreshold)
    *folResp *= params.frozenSoilFolREff; // allows foliar resp. to be shutdown by a given fraction in winter

  *woodResp = params.baseVegResp * envi.plantWoodC * pow(params.vegRespQ10, climate->tair/10.0);
  *growthResp = params.growthRespFrac * getMeanTrackerMean(meanNPP); // Rg is a fraction of the recent mean NPP
  
  if (*growthResp < 0)
    *growthResp = 0;
}
  

// !!! general, multi-purpose functions !!!


// general function to calculate soil C broken down, as a function of tsoil and soil moisture:
// (g C * m^-2 * day^-1)
// based on function in PnET-CN
// poolC is amt. of C in this soil pool (g C * m^-2), 
// baseRate is breakdown rate at 0 degrees C, max soil moisture (g respired/g/day)
// water and whc (both in cm) refer to this specific soil layer
double soilBreakdown(double poolC, double baseRate, double water, double whc, double tsoil, double Q10) {
  double tempEffect;
  double moistEffect;

#if DAYCENT_WATER_HRESP
  // calculate here so only have to do this ugly calculation once
  static const double daycentWaterExp = 3.22 * (1.7 - 0.55)/(0.55 + 0.007); 
#endif


#if LLOYD_TAYLOR // we're using the Lloyd-Taylor model for temperature effect
  tempEffect = exp(params.E0 * (-1/params.T0 - 1/(tsoil - params.T0)));
#else // using standard Q10
  tempEffect = pow(Q10, tsoil/10.0);
#endif

#if WATER_HRESP // if soil moisture affects heterotrophic resp.

#if DAYCENT_WATER_HRESP // using DAYCENT formulation
  // NOTE: would probably be best to make all these constants parameters that are read in, but for now we have them hard-coded
  moistEffect = pow(((water/whc - 1.7)/(0.55 - 1.7)), daycentWaterExp)
    * pow((water/whc + 0.007)/(0.55 + 0.007), 3.22);
#else // using PnET formulation
  moistEffect = pow((water/whc), params.soilRespMoistEffect);
#endif // DAYCENT_WATER_HRESP

#else // no WATER_HRESP
  moistEffect = 1;
#endif // WATER_HRESP

  return poolC * baseRate * tempEffect * moistEffect;
}
  

// !!! functions to calculate fluxes !!!
// all fluxes are on a per-day basis


// transfer of carbon from plant woody material to litter, in g C * m^-2 ground area * day^-1
// (includes above-ground and roots)
double woodLitterF(double plantWoodC) {
  return plantWoodC * params.turnoverRate; // turnover rate is fraction lost per day
}


void calculateFluxes() {
  // auxiliary variables:
  double baseFolResp;
  double potGrossPsn; // potential photosynthesis, without water stress
  double dWater;
  double lai; // m^2 leaf/m^2 ground (calculated from plantLeafC)
  double litterBreakdown; /* total litter breakdown (i.e. litterToSoil + rLitter) 
							 (g C/m^2 ground/day) */
  double folResp, woodResp; // maintenance respiration terms, g C * m^-2 ground area * day^-1
  double litterWater, soilWater; /* amount of water in litter and soil (cm)
				    taken from either environment or climate drivers, depending on value of MODEL_WATER */
#if GROWTH_RESP
  double growthResp; // g C * m^-2 ground area * day^-1
#endif

#if COMPLEX_WATER
  double netRain; // rain - immedEvap (cm/day) 
#endif


#if MODEL_WATER // take litterWater and soilWater from environment
  litterWater = envi.litterWater;
  soilWater = envi.soilWater;
#else // take litterWater and soilWater from climate drivers
  litterWater = climate->soilWetness * params.litterWHC; /* assume wetness is uniform throughout all layers
							    (probably unrealistic, but it shouldn't matter too much) */
  soilWater = climate->soilWetness * params.soilWHC;
#endif

  lai = envi.plantLeafC / params.leafCSpWt; // current lai

  potPsn(&potGrossPsn, &baseFolResp, lai, climate->tair, climate->vpd, climate->par, climate->day);
  moisture(&(fluxes.transpiration), &dWater, potGrossPsn, climate->vpd, soilWater);

#if MODEL_WATER // water modeling happens here:

#if COMPLEX_WATER
  calcPrecip(&(fluxes.rain), &(fluxes.snowFall), &(fluxes.immedEvap));
  netRain = fluxes.rain - fluxes.immedEvap;
  snowPack(&(fluxes.snowMelt), &(fluxes.sublimation), fluxes.snowFall);
  soilWaterFluxes(&(fluxes.fastFlow), &(fluxes.evaporation), &(fluxes.topDrainage), &(fluxes.bottomDrainage),
		  netRain, fluxes.snowMelt, fluxes.transpiration, litterWater, soilWater);
#else
  simpleWaterFlow(&(fluxes.rain), &(fluxes.snowFall), &(fluxes.immedEvap), &(fluxes.snowMelt), &(fluxes.sublimation),
		  &(fluxes.fastFlow), &(fluxes.evaporation), &(fluxes.topDrainage), &(fluxes.bottomDrainage), 
		  soilWater, envi.snow, climate->precip, climate->tair, climate->length, fluxes.transpiration);
#endif // COMPLEX_WATER

#else // MODEL_WATER = 0: set all water fluxes to 0
  fluxes.rain = fluxes.snowFall = fluxes.immedEvap = fluxes.snowMelt = fluxes.sublimation
    = fluxes.fastFlow = fluxes.evaporation = fluxes.topDrainage = fluxes.bottomDrainage = 0;
#endif // MODEL_WATER

  getGpp(&(fluxes.photosynthesis), potGrossPsn, dWater);

#if GROWTH_RESP
  vegResp2(&folResp, &woodResp, &growthResp, baseFolResp, fluxes.photosynthesis);
  fluxes.rVeg = folResp + woodResp + growthResp;
#else
  vegResp(&folResp, &woodResp, baseFolResp);
  fluxes.rVeg = folResp + woodResp;
#endif

  leafFluxes(&(fluxes.leafCreation), &(fluxes.leafLitter), envi.plantLeafC);
  
#if LITTER_POOL
  litterBreakdown = soilBreakdown(envi.litter, params.litterBreakdownRate, 
				  litterWater, params.litterWHC, climate->tsoil, params.soilRespQ10);
  // NOTE: right now, we don't have capability to use separate cold soil params for litter
#else
  litterBreakdown = 0;
#endif
  
  // finally, calculate fluxes that we haven't already calculated:

  fluxes.woodLitter = woodLitterF(envi.plantWoodC);

  // the following two fluxes will be 0 if we're not modeling litter pool:
  fluxes.rLitter = litterBreakdown * params.fracLitterRespired;
  fluxes.litterToSoil = litterBreakdown * (1.0 - params.fracLitterRespired);


#if SEASONAL_R_SOIL // decide which parameters to use based on tsoil
  if (climate->tsoil >= params.coldSoilThreshold) // use normal (warm temp.) params
    fluxes.rSoil = soilBreakdown(envi.soil, params.baseSoilResp, 
				 soilWater, params.soilWHC, climate->tsoil, params.soilRespQ10);
  else // use cold temp. params
    fluxes.rSoil = soilBreakdown(envi.soil, params.baseSoilRespCold,
				 soilWater, params.soilWHC, climate->tsoil, params.soilRespQ10Cold);
#else // SEASONAL_R_SOIL FALSE -> always use normal params
  fluxes.rSoil = soilBreakdown(envi.soil, params.baseSoilResp, 
			       soilWater, params.soilWHC, climate->tsoil, params.soilRespQ10);
#endif

  // printf("%f %f %f\n", fluxes.rLitter*climate->length, fluxes.rSoil*climate->length, (fluxes.leafLitter+fluxes.woodLitter)*climate->length);

  // printf("%f %f %f %f %f\n", fluxes.photosynthesis*climate->length, folResp*climate->length, woodResp*climate->length, fluxes.rSoil*climate->length, fluxes.rLitter*climate->length);

  // printf("%f %f %f %f %f\n", fluxes.leafLitter*climate->length, fluxes.woodLitter*climate->length, fluxes.rVeg*climate->length, fluxes.rSoil*climate->length, fluxes.photosynthesis*climate->length);

  /* diagnosis: print water fluxes:
  printf("%f %f %f %f %f %f %f %f %f %f\n", 
	 fluxes.rain*climate->length, fluxes.snowFall*climate->length, fluxes.immedEvap*climate->length,
	 fluxes.snowMelt*climate->length, fluxes.sublimation*climate->length,
	 fluxes.fastFlow*climate->length, fluxes.evaporation*climate->length, fluxes.topDrainage*climate->length,
	 fluxes.bottomDrainage*climate->length, fluxes.transpiration*climate->length);
  */

  /* printf("%f %f %f\n", fluxes.photosynthesis*climate->length, fluxes.transpiration*climate->length, 
	 (fluxes.transpiration > 0) ? (fluxes.photosynthesis/fluxes.transpiration) : 0);
  */

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
  trackers.soilWetnessFrac = envi.soilWater/params.soilWHC;
}

// update trackers at each time step
// oldSoilWater is how much soil water there was at the beginning of the time step (cm)
void updateTrackers(double oldSoilWater) {
  static int lastYear = -1; // what was the year of the last step?

  if (climate->year != lastYear) { // new year: reset yearly trackers
    trackers.yearlyGpp = 0.0;
    trackers.yearlyRtot = 0.0;
    trackers.yearlyRa = 0.0;
    trackers.yearlyRh = 0.0;
    trackers.yearlyNpp = 0.0;
    trackers.yearlyNee = 0.0;

    lastYear = climate->year;


    // At start of 1999, reset cumulative trackers
    // Note that this is only for one specific application: we don't usually want to do this
    /*
    if (climate->year == 1999) {
      trackers.totGpp = 0.0;
      trackers.totRtot = 0.0;
      trackers.totRa = 0.0;
      trackers.totRh = 0.0;
      trackers.totNpp = 0.0;
      trackers.totNee = 0.0;
    }
    */
  }

  trackers.gpp = fluxes.photosynthesis * climate->length;
  trackers.ra = fluxes.rVeg * climate->length;
  trackers.rh = (fluxes.rLitter + fluxes.rSoil) * climate->length;
  trackers.rtot = trackers.ra + trackers.rh;
  trackers.npp = trackers.gpp - trackers.ra;
  trackers.nee = -1.0*(trackers.npp - trackers.rh);

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
  
  trackers.evapotranspiration = (fluxes.transpiration + fluxes.immedEvap + fluxes.evaporation + fluxes.sublimation)
    * climate->length;

  trackers.soilWetnessFrac = (oldSoilWater + envi.soilWater)/(2.0*params.soilWHC);
  // mean of soil wetness at start of time step and soil wetness at end of time step - assume linear
}


// If var < minVal, then set var = 0
// Note that if minVal = 0, then this will (as suggested) ensure that var >= 0
//  If minVal > 0, then minVal can be thought of as some epsilon value, below which var is treated as 0
void ensureNonNegative(double *var, double minVal) {
  if (*var < minVal)
    *var = 0.;
}


// Make sure all environment variables are positive after updating them:
// Note: For some variables, there are checks elsewhere in the code to ensure that fluxes don't make stocks go negative
//  However, the stocks could still go slightly negative due to rounding errors
// For other variables, there are NOT currently (as of 7-16-06) checks to make sure out-fluxes aren't too large
//  In these cases, this function should be thought of as a last-resort check - ideally, the fluxes would be modified 
//  so that they did not make the stocks negative (otherwise the fluxes could be inconsistent with the changes in the stocks)
void ensureNonNegativeStocks() {

  ensureNonNegative(&(envi.plantWoodC), 0);
  ensureNonNegative(&(envi.plantLeafC), 0);

#if LITTER_POOL
  ensureNonNegative(&(envi.litter), 0);
#endif

  ensureNonNegative(&(envi.soil), 0);

#if MODEL_WATER

#if LITTER_WATER
  ensureNonNegative(&(envi.litterWater), 0);
#endif

  ensureNonNegative(&(envi.soilWater), 0);
  ensureNonNegative(&(envi.snow), TINY); /* In the case of snow, the model has very different behavior for a snow pack of 0
					 vs. a snow pack of slightly greater than 0 (e.g. no soil evaporation if snow > 0).
					 Thus to avoid large errors due to small rounding errors, we'll set snow = 0 any time it falls below TINY,
					 the assumption being that if snow < TINY, then it was really supposed to be 0, but isn't because of rounding errors.
				      */
#endif

}					 

// !!! main runner function !!!


// calculate all fluxes and update state for this time step
// we calculate all fluxes before updating state in case flux calculations depend on the old state
void updateState() {
  double npp; // net primary productivity, g C * m^-2 ground area * day^-1
  double oldSoilWater; // how much soil water was there before we updated it? Used in trackers

  oldSoilWater = envi.soilWater;

  calculateFluxes();

  // update the stocks, with fluxes adjusted for length of time step:
  envi.plantWoodC += (fluxes.photosynthesis - fluxes.leafCreation - fluxes.woodLitter - fluxes.rVeg)
	* climate->length;
  envi.plantLeafC += (fluxes.leafCreation - fluxes.leafLitter) * climate->length;

#if LITTER_POOL
  envi.litter += (fluxes.woodLitter + fluxes.leafLitter - fluxes.litterToSoil - fluxes.rLitter)
	* climate->length;
  envi.soil += (fluxes.litterToSoil - fluxes.rSoil) * climate->length;
#else
  envi.soil += (fluxes.woodLitter + fluxes.leafLitter - fluxes.rSoil) * climate->length;
#endif

#if MODEL_WATER // water pool updating happens here:

#if LITTER_WATER // (2 soil water layers; litter water will only be on if complex water is also on)
  envi.litterWater += (fluxes.rain + fluxes.snowMelt - fluxes.immedEvap - fluxes.fastFlow
		       - fluxes.evaporation - fluxes.topDrainage) * climate->length;
  envi.soilWater += (fluxes.topDrainage - fluxes.transpiration - fluxes.bottomDrainage)
    * climate->length;
#else // LITTER_WATER = 0 (only one soil water layer)
  // note: some of these fluxes will always be 0 if complex water is off
  envi.soilWater += (fluxes.rain + fluxes.snowMelt - fluxes.immedEvap - fluxes.fastFlow
		     - fluxes.evaporation - fluxes.transpiration - fluxes.bottomDrainage) * climate->length;
#endif // LITTER_WATER

  // if COMPLEX_WATER = 0 or SNOW = 0, some or all of these fluxes will always be 0
  envi.snow += (fluxes.snowFall - fluxes.snowMelt - fluxes.sublimation) * climate->length;

#endif // MODEL_WATER

  ensureNonNegativeStocks();

  npp = fluxes.photosynthesis - fluxes.rVeg;
  addValueToMeanTracker(meanNPP, npp, climate->length); // update running mean of NPP
  updateTrackers(oldSoilWater);
}


// initialize phenology tracker structure, based on day of year of first climate record
// (have the leaves come on yet this year? have they fallen off yet this year?)
void initPhenologyTrackers() {
  
  phenologyTrackers.didLeafGrowth = pastLeafGrowth(); // first year: have we passed growing season start date?
  phenologyTrackers.didLeafFall = pastLeafFall(); // first year: have we passed growing season end date?
  
  /* if we think we've done leaf fall this year but not leaf growth, something's wrong
     this could happen if, e.g. we're using soil temp-based leaf growth, 
     and soil temp. on first day doesn't happen to pass threshold
     fix this by setting didLeafGrowth to 1 in this case
     Note: this won't catch all potential problems (e.g. with soil temp-based leaf growth,
     temp. on first day may not pass threshold, but it may not yet be the end of the growing season;
     in this case we could accidentally grow the leaves again once the temp. rises past the threshold again.
     Also, we'll have problems with GDD-based growth if first day of simulation is not Jan. 1.)
  */
  if (phenologyTrackers.didLeafFall && !phenologyTrackers.didLeafGrowth)
    phenologyTrackers.didLeafGrowth = 1;

  phenologyTrackers.lastYear = climate->year; // set the year of the previous (non-existent) time step to be this year
}


// Setup model to run at given location (0-indexing: if only one location, loc should be 0)
void setupModel(SpatialParams *spatialParams, int loc) {

  // load parameters into global param structure: spatialParams was told where to put values in readParamData
  loadSpatialParams(spatialParams, loc); 

  // a test: use constant (measured) soil respiration:
  // make it so soil resp. is 5.2 g C m-2 day-1 at 10 degrees C, moisture-saturated soil, and soil C = init. soil C
  // params.baseSoilResp = ((5.2 * 365.0)/params.soilInit)/params.soilRespQ10;

  // change units of parameters:
  params.baseVegResp /= 365.0; // change from per-year to per-day rate
  params.litterBreakdownRate /= 365.0;
  params.baseSoilResp /= 365.0;
  params.baseSoilRespCold /= 365.0;
  params.turnoverRate /= 365.0;
  params.leafTurnoverRate /= 365.0; 

  // calculate additional parameters:
  params.psnTMax = params.psnTOpt + (params.psnTOpt - params.psnTMin); // assumed symmetrical

  envi.plantWoodC = params.plantWoodInit;
  envi.plantLeafC = params.laiInit * params.leafCSpWt;
  envi.litter = params.litterInit;
  envi.soil = params.soilInit;

  envi.litterWater = params.litterWFracInit * params.litterWHC;
  if (envi.litterWater < 0)
    envi.litterWater = 0;
  else if (envi.litterWater > params.litterWHC)
    envi.litterWater = params.litterWHC;

  envi.soilWater = params.soilWFracInit * params.soilWHC;
  if (envi.soilWater < 0)
    envi.soilWater = 0;
  else if (envi.soilWater > params.soilWHC)
    envi.soilWater = params.soilWHC;

  envi.snow = params.snowInit;

  if (firstClimates[loc] != NULL)
    climate = firstClimates[loc]; // set climate ptr to point to first climate record in this location
  else // no climate data for this location
    climate = firstClimates[0]; // use climate data from location 0
  initTrackers();
  initPhenologyTrackers();
  resetMeanTracker(meanNPP, 0); // initialize with mean NPP (over last MEAN_NPP_DAYS) of 0
}


/* do one run of the model using parameter values in spatialParams, output results to out
   If printHeader = 1, print a header for the output file, if 0 don't
   Run at spatial location given by loc (0-indexing) - or run everywhere if loc = -1
   Note: number of locations given in spatialParams
*/
void runModelOutput(FILE *out, int printHeader, SpatialParams *spatialParams, int loc) {
  int firstLoc, lastLoc, currLoc;
  
  if (printHeader)
    outputHeader(out);

  if (loc == -1) { // run everywhere
    firstLoc = 0;
    lastLoc = spatialParams->numLocs - 1;
  }
  else // just run at one point
    firstLoc = lastLoc = loc;
  
  for (currLoc = firstLoc; currLoc <= lastLoc; currLoc++) {
    setupModel(spatialParams, currLoc);
      
    while (climate != NULL) {
      updateState();
      outputState(out, currLoc, climate->year, climate->day, climate->time);
      climate = climate->nextClim;
    }
  }
}



/* pre: outArray has dimensions of at least (# model steps) x numDataTypes
   dataTypeIndices[0..numDataTypes-1] gives indices of data types to use (see DATA_TYPES array in sipnet.h)
   
   run model with parameter values in spatialParams, don't output to file
   instead, output some variables at each step into an array
   Run at spatial location given by loc (0-indexing)
   Note: can only run at one location: to run at all locations, must put runModelNoOut call in a loop
*/
void runModelNoOut(double **outArray, int numDataTypes, int dataTypeIndices[], SpatialParams *spatialParams, int loc) {
  int step = 0;
  int outputNum;

  setupModel(spatialParams, loc);

  // loop through every step of the model:
  while (climate != NULL) {
    updateState();
    
    // loop through all desired outputs, putting each into outArray:
    for (outputNum = 0; outputNum < numDataTypes; outputNum++)
      outArray[step][outputNum] = *(outputPtrs[dataTypeIndices[outputNum]]);

    step++;
    climate = climate->nextClim;
  }
}

// do a sensitivity test on paramNum, varying from low to high, doing a total of numRuns runs
// run only at a single location (given by loc)
void sensTest(FILE *out, int paramNum, double low, double high, int numRuns, SpatialParams *spatialParams, int loc) 
{
  int runNum;
  double changeAmt = (high - low)/(double)(numRuns - 1);
  double value; // current parameter value
  
  value = low;
  for (runNum = 0; runNum < numRuns; runNum++) {
    // printf("%d %f\n", runNum, value);
    // fprintf(out, "# Value = %f\n", value);
    setSpatialParam(spatialParams, paramNum, loc, value);
    runModelOutput(out, 0, spatialParams, loc);
    fprintf(out, "\n\n");
    value += changeAmt;
  }
}


// write to file which model components are turned on
// (i.e. the value of the #DEFINE's at the top of file)
// pre: out is open for writing
void printModelComponents(FILE *out) {
  fprintf(out, "Optional model components (0 = off, 1 = on):\n");
  fprintf(out, "GROWTH_RESP = %d\n", GROWTH_RESP);
  fprintf(out, "LITTER_POOL = %d\n", LITTER_POOL);
  fprintf(out, "LLOYD_TAYLOR = %d\n", LLOYD_TAYLOR);
  fprintf(out, "SEASONAL_R_SOIL = %d\n", SEASONAL_R_SOIL);
  fprintf(out, "WATER_PSN = %d\n", WATER_PSN);
  fprintf(out, "WATER_HRESP = %d\n", WATER_HRESP);
  fprintf(out, "DAYCENT_WATER_HRESP = %d\n", DAYCENT_WATER_HRESP);
  fprintf(out, "MODEL_WATER = %d\n", MODEL_WATER);
  fprintf(out, "COMPLEX_WATER = %d\n", COMPLEX_WATER);
  fprintf(out, "LITTER_WATER = %d\n", LITTER_WATER);
  fprintf(out, "LITTER_WATER_DRAINAGE = %d\n", LITTER_WATER_DRAINAGE);
  fprintf(out, "SNOW = %d\n", SNOW);
  fprintf(out, "GDD = %d\n", GDD);
  fprintf(out, "SOIL_PHENOL = %d\n", SOIL_PHENOL);
  fprintf(out, "\n");
} 


// NOTE: if change # of data types below, be sure to change MAX_DATA_TYPES in sipnet.h
// other than that, to change return-able data types,
// just have to change arrays in getDataTypeNames and setupOutputPointers

// Can get some extra data types by defining EXTRA_DATA_TYPES in sipnet.h
// Note that using extra data types will break the estimate program, since there will be the wrong # of columns in the .dat file,
//  but this can be used for outputting extra data types when computing means and standard dev's across a number of param. sets

// return an array[0..MAX_DATA_TYPES-1] of strings,
// where arr[i] gives the name of data type i
char **getDataTypeNames() {
#if EXTRA_DATA_TYPES
  static char *DATA_TYPES[MAX_DATA_TYPES] = {"EVAPOTRANSPIRATION", "NEE", "SOIL WETNESS",
					     "GPP", "RTOT", "RA", "RH", "NPP",
					     "YEARLY_GPP", "YEARLY_RTOT", "YEARLY_RA", "YEARLY_RH", "YEARLY_NPP", "YEARLY_NEE",
					     "TOT_GPP", "TOT_RTOT", "TOT_RA", "TOT_RH", "TOT_NPP", "TOT_NEE"};
#else
  static char *DATA_TYPES[MAX_DATA_TYPES] = {"EVAPOTRANSPIRATION", "NEE", "SOIL WETNESS"};
#endif

  return DATA_TYPES;
}    

// set outputPtrs array to point to appropriate values
void setupOutputPointers(void) {
  int i; // keep track of current index into array

  i = 0;
  // we post-increment i every time we assign an array element
  outputPtrs[i++] = &(trackers.evapotranspiration);
  outputPtrs[i++] = &(trackers.nee);
  outputPtrs[i++] = &(trackers.soilWetnessFrac);
#if EXTRA_DATA_TYPES
  outputPtrs[i++] = &(trackers.gpp);
  outputPtrs[i++] = &(trackers.rtot);
  outputPtrs[i++] = &(trackers.ra);
  outputPtrs[i++] = &(trackers.rh);
  outputPtrs[i++] = &(trackers.npp);

  outputPtrs[i++] = &(trackers.yearlyGpp);
  outputPtrs[i++] = &(trackers.yearlyRtot);
  outputPtrs[i++] = &(trackers.yearlyRa);
  outputPtrs[i++] = &(trackers.yearlyRh);
  outputPtrs[i++] = &(trackers.yearlyNpp);
  outputPtrs[i++] = &(trackers.yearlyNee);

  outputPtrs[i++] = &(trackers.totGpp);
  outputPtrs[i++] = &(trackers.totRtot);
  outputPtrs[i++] = &(trackers.totRa);
  outputPtrs[i++] = &(trackers.totRh);
  outputPtrs[i++] = &(trackers.totNpp);
  outputPtrs[i++] = &(trackers.totNee);
#endif
}


/* do initializations that only have to be done once for all model runs:
   read in climate data and initial parameter values
   parameter values get stored in spatialParams (along with other parameter information),
   which gets allocated and initialized here (thus requiring that spatialParams is passed in as a pointer to a pointer)
   number of time steps in each location gets stored in steps vector, which gets dynamically allocated with malloc
   (steps must be a pointer to a pointer so it can be malloc'ed)

   also set up pointers to different output data types
   and setup meanNPP tracker

   initModel returns number of spatial locations

   paramFile is parameter data file
   paramFile-spatial is file with parameter values of spatially-varying parameters (1st line contains number of locations)
   climFile is climate data file
*/
int initModel(SpatialParams **spatialParams, int **steps, char *paramFile, char *climFile) {
  char spatialParamFile[256];
  int numLocs;

  strcpy(spatialParamFile, paramFile);
  strcat(spatialParamFile, "-spatial");

  numLocs = readParamData(spatialParams, paramFile, spatialParamFile);
  *steps = readClimData(climFile, numLocs);

  setupOutputPointers();

  meanNPP = newMeanTracker(0, MEAN_NPP_DAYS, MEAN_NPP_MAX_ENTRIES); 

  return numLocs;
}

  

// call this when done running model:
// de-allocates space for climate linked list
// (needs to know number of locations)
void cleanupModel(int numLocs) {
  freeClimateList(numLocs);
  deallocateMeanTracker(meanNPP);
}   

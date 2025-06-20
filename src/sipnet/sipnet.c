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

#include "common/util.h"

#include "sipnet.h"
#include "runmean.h"
#include "outputItems.h"
#include "modelStructures.h"
#include "events.h"
#include "common/exitCodes.h"

// begin definitions for choosing different model structures
// (1 -> true, 0 -> false)

// alternative transpiration methods modified by Dave Moore
#define ALTERNATIVE_TRANS 0
// do we want to implement alternative transpiration?

#define BALL_BERRY 0
// implement a Ball Berry submodel to calculate gs from RH, CO2 and A
// MUST BE OFF for PENMAN MONTEITH TO RUN

#define PENMAN_MONTEITH_TRANS 0
// implement a transpiration calculation based on the Penman-Monteith Equation.
// March 1st 2007 PM equation not really working.

// #define G 0
// assume that soil heat flux is zero;

#define GROWTH_RESP 0
// explicitly model growth resp., rather than including with maint. resp.

#define LLOYD_TAYLOR 0
// use Lloyd-Taylor model for soil respiration, in which temperature sensitivity
// decreases at higher temperatures? Lloyd-Taylor model is R = R0 * e^(E0 *
// (1/(-T0) - 1/(T - T0))) (see Lloyd and Taylor 1994, "On the temperature
// dependence of soil respiration") where R0 is rate at 0 degrees C, T is soil
// temp., E0 and T0 are parameters

#define SEASONAL_R_SOIL 0 && !LLOYD_TAYLOR
// use different parameters for soil resp. (baseSoilResp and soilRespQ10) when
// tsoil < (some threshold)? if so, use standard parameters for warm soil,
// separate parameters for cold soil if we're using the Lloyd-Taylor model, we
// won't use different parameters at different temperatures

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
// do we use a more complex water submodel? (model evaporation as well as
// transpiration) when we use a complex water submodel, we always model snow if
// model water is off, then complex water is off: complex water wouldn't do
// anything

#define LITTER_WATER 0 && (COMPLEX_WATER)
// do we have a separate litter water layer, used for evaporation?
// if complex water is off, then litter water is off: litter water layer
// wouldn't do anything

#if LITTER_WATER & EVENT_HANDLER
#error EVENT_HANDLER and LITTER_WATER may not both be activated
#endif
// We do not handle having both LITTER_WATER and EVENT_HANDLING on; this may
// be implemented in a later phase

#define LITTER_WATER_DRAINAGE 1 && (LITTER_WATER)
// does water from the top layer drain down into bottom layer even if top layer
// not overflowing? if litter water is off, then litter water drainage is off:
// litter water drainage wouldn't do anything

#define LEAF_WATER 0 && (COMPLEX_WATER)
// calculate leaf pool and evaporate from that pool
// makes immediate evaporation more realistic for smaller timesteps

#define SNOW (1 || (COMPLEX_WATER)) && MODEL_WATER
// keep track of snowpack, rather than assuming all precip. is liquid
// note: when using a complex water submodel, we ALWAYS keep track of snowpack
// if model water is off, then snow is off: snow wouldn't do anything

#define GDD 1
// use GDD to determine leaf growth? (note: mutually exclusive with SOIL_PHENOL)

#define SOIL_PHENOL 0 && !GDD
// use soil temp. to determine leaf growth? (note: mutually exclusive with GDD)

// LITTER_POOL moved to modelStructures.h
// have extra litter pool, in addition to soil c pool

#define SOIL_MULTIPOOL 0 && !LITTER_POOL
// do we have a multipool approach to model soils?
// if LITTER_POOL == 1, then SOIL_MULTIPOOL will be 0 because we take care of
// litter with the soil quality submodel.

#define NUMBER_SOIL_CARBON_POOLS 3
// first number: number of pools we want to have.
// IF SOIL_MULTIPOOL=0, then NUMBER_SOIL_CARBON_POOLS = 1
// if SOIL_MULTIPOOL=1, then NUMBER_SOIL_CARBON_POOLS = number given

#define SOIL_QUALITY 0 && SOIL_MULTIPOOL
// do we have a soil quality submodel?
// we only do SOIL_QUALITY if SOIL_MULTIPOOL is turned on

#define MICROBES 0 && !SOIL_MULTIPOOL
// do we utilize microbes.  This will only be an option
// if SOIL_MULTIPOOL==0 and MICROBES ==1

#define STOICHIOMETRY 0 && MICROBES
// do we utilize stoichometric considerations for the microbial pool?

// ROOTS moved to modelStructures.h
// do we model root dynamics?

// end definitions for choosing different model structures

// begin constant definitions

#define C_WEIGHT 12.0  // molecular weight of carbon
#define TEN_6 1000000.0  // for conversions from micro
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

// constants for tracking running mean of fPAR:
#define MEAN_FPAR_DAYS 1  // over how many days do we keep the running mean?
#define MEAN_FPAR_MAX_ENTRIES (MEAN_FPAR_DAYS * 24)  //
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

// linked list of climate variables
// one node for each time step
// these climate data are read in from a file

typedef struct ClimateVars ClimateNode;

struct ClimateVars {
  int year;  // year of start of this timestep
  int day;  // day of start of this timestep (1 = Jan 1.)
  double time;  // time of start of this timestep (hour.fraction - e.g. noon
                // = 12.0, midnight = 0.0)
  double length;  // length of this timestep (in days) - allow variable-length
                  // timesteps
  double tair;  // avg. air temp for this time step (degrees C)
  double tsoil;  // avg. soil temp for this time step (degrees C)
  double par; /* average par for this time step (Einsteins * m^-2 ground area *
    day^-1) NOTE: input is in Einsteins * m^-2 ground area, summed over entire
    time step */
  double precip; /* total precip. for this time step (cm water equiv. - either
        rain or snow) NOTE: input is in mm */
  double vpd; /* average vapor pressure deficit (kPa)
     NOTE: input is in Pa */
  double vpdSoil; /* average vapor pressure deficit between soil and air (kPa)
         NOTE: input is in Pa
         differs from vpd in that saturation vapor pressure calculated using
         Tsoil rather than Tair */
  double vPress; /* average vapor pressure in canopy airspace (kPa)
        NOTE: input is in Pa */
  double wspd;  // avg. wind speed (m/s)
  double soilWetness;  // fractional soil wetness (fraction of saturation -
                       // between 0 and 1)

#if GDD
  double gdd; /* growing degree days from Jan. 1 to now
     NOTE: Calculated, *not* read from file */
#endif

  ClimateNode *nextClim;
};

#define NUM_CLIM_FILE_COLS 13

// model parameters which can change from one run to the next
// these include initializations of state
// initial values are read in from a file, or calculated at start of model
// if any parameters are added here, and these parameters are to be read from
// file,
//  be sure to add them to the readParamData function, below
typedef struct Parameters {
  // parameters read in from file:
  // initial state values:
  double plantWoodInit;  // g C * m^-2 ground area in wood (above-ground +
                         // roots)
  double laiInit;  // initial leaf area, m^2 leaves * m^-2 ground area (multiply
                   // by leafCSpWt to get initial plant leaf C)
  double litterInit;  // g C * m^-2 ground area
  double soilInit;  // g C * m^-2 ground area
  double litterWFracInit;  // unitless: fraction of litterWHC
  double soilWFracInit;  // unitless: fraction of soilWHC
  double snowInit;  // cm water equiv.

  // 7 parameters
  // parameters:

  // photosynthesis:
  double aMax; /* max photosynthesis (nmol CO2 * g^-1 leaf * sec^-1)
     assuming max. possible par, all intercepted, no temp, water or vpd stress
   */
  double aMaxFrac;  // avg. daily aMax as fraction of instantaneous
  double baseFolRespFrac;  // basal foliage resp. rate, as % of max. net
                           // photosynth. rate
  double psnTMin, psnTOpt;  // min and optimal temps at which net photosynthesis
                            // occurs (degrees C)
  double dVpdSlope, dVpdExp;  // dVpd = 1 - dVpdSlope * vpd^dVpdExp
  double halfSatPar; /* par at which photosynthesis occurs at 1/2 theoretical
            maximum (Einsteins * m^-2 ground area * day^-1) */
  double attenuation;  // light attenuation coefficient

  // 9 parameters

  // phenology-related:
  double leafOnDay;  // day when leaves appear
  double gddLeafOn;  // with gdd-based phenology, gdd threshold for leaf
                     // appearance
  double soilTempLeafOn;  // with soil temp-based phenology, soil temp threshold
                          // for leaf appearance
  double leafOffDay;  // day when leaves disappear
  double leafGrowth;  // add'l leaf growth at start of growing season (g C *
                      // m^-2 ground)
  double fracLeafFall;  // add'l fraction of leaves that fall at end of growing
                        // season
  double leafAllocation;  // fraction of NPP allocated to leaf growth
  double leafTurnoverRate; /* average turnover rate of leaves, in fraction per
            day NOTE: read in as per-year rate! */

  // 8 parameters
  // autotrophic respiration:
  double baseVegResp; /* vegetation maintenance respiration at 0 degrees C
      (g C respired * g^-1 plant C * day^-1)
      NOTE: only counts plant wood C - leaves handled elsewhere
      (both above and below-ground: assumed for now to have same resp. rate)
      NOTE: read in as per-year rate! */
  double vegRespQ10;  // scalar determining effect of temp on veg. resp.
  double growthRespFrac;  // growth resp. as fraction of (GPP - woodResp -
                          // folResp)
  double frozenSoilFolREff;  // amount that foliar resp. is shutdown if soil is
                             // frozen (0 = full shutdown, 1 = no shutdown)
  double frozenSoilThreshold;  // soil temperature below which frozenSoilFolREff
                               // and frozenSoilEff kick in (degrees C)

  // 5 parameters
  // soil respiration:
  double litterBreakdownRate; /* rate at which litter is converted to
                 soil/respired at 0 degrees C and max soil moisture (g C broken
                 down * g^-1 litter C * day^-1) NOTE: read in as per-year rate
               */
  double fracLitterRespired; /* of the litter broken down, fraction respired
                (the rest is transferred to soil pool) */
  double baseSoilResp; /* soil respiration at 0 degrees C and max soil moisture
       (g C respired * g^-1 soil C * day^-1)
       NOTE: read in as per-year rate! */
  double baseSoilRespCold; /* soil respiration at 0 degrees C and max soil
             moisture when tsoil < coldSoilThreshold (g C respired * g^-1 soil C
             * day^-1) NOTE: read in as per-year rate! */

  // 6 parameters

  double soilRespQ10;  // scalar determining effect of temp on soil resp.
  double soilRespQ10Cold;  // scalar determining effect of temp on soil resp.
                           // when tsoil < coldSoilThreshold

  double coldSoilThreshold;  // temp. at which use baseSoilRespCold and
                             // soilRespQ10Cold (if SEASONAL_R_SOIL true)
                             // (degrees C)
  double E0;  // E0 in Lloyd-Taylor soil respiration function
  double T0;  // T0 in Lloyd-Taylor soil respiration function
  double soilRespMoistEffect;  // scalar determining effect of moisture on soil
                               // resp.

  // 8 parameters
  // moisture-related:
  double waterRemoveFrac; /* fraction of plant available soil water which can be
          removed in one day without water stress occurring */
  double frozenSoilEff;  // fraction of water that's available if soil is frozen
                         // (0 = none available, 1 = all still avail.)
                         // NOTE: if frozenSoilEff = 0, then shut down psn. even
                         // if WATER_PSN = 0, if soil is frozen (if
                         // frozenSoilEff > 0, it has no effect if WATER_PSN =
                         // 0)
  double wueConst;  // water use efficiency constant
  double litterWHC;  // litter (evaporative layer) water holding capacity (cm)
  double soilWHC;  // soil (transpiration layer) water holding capacity (cm)
  double immedEvapFrac;  // fraction of rain that is immediately intercepted &
                         // evaporated
  double fastFlowFrac;  // fraction of water entering soil that goes directly to
                        // drainage
  double snowMelt;  // rate at which snow melts (cm water equiv./degree C/day)
  double litWaterDrainRate;  // rate at which litter water drains into lower
                             // layer when litter layer fully moisture-saturated
                             // (cm water/day)
  double rdConst;  // scalar determining amount of aerodynamic resistance
  double rSoilConst1, rSoilConst2;  // soil resistance =
                                    // e^(rSoilConst1 - rSoilConst2 * W1)
                                    // where W1 = (litterWater/litterWHC)
  double m_ballBerry;  // slope for the Ball Berry relationship
  double leafCSpWt;  // g C * m^-2 leaf area
  double cFracLeaf;  // g leaf C * g^-1 leaf
  double leafPoolDepth;  // leaf (evaporative) pool rim thickness in mm

  double woodTurnoverRate;  // average turnover rate of woody plant C, in
                            // fraction per day (leaf loss handled separately)
                            // NOTE: read in as per-year rate!

  // calculated parameters:
  double psnTMax;  // degrees C - assumed symmetrical around psnTOpt

  // 16-1 calculated= 15 parameters

  // quality model parameters
  double qualityLeaf;  // value for leaf litter quality
  double qualityWood;  // value for wood litter quality
  double efficiency;  // conversion efficiency of ingested carbon

  // 4 parameters

  double maxIngestionRate;  // hr-1 - maximum ingestion rate of the microbe
  double halfSatIngestion;  // mg C g-1 soil - half saturation ingestion rate of
                            // microbe
  double totNitrogen;  // Percentage nitrogen in soil
  double microbeNC;  // mg N / mg C - microbe N:C ratio
  // 5 parameters

  double microbeInit;  // mg C / g soil microbe initial carbon amount
                       // 1 parameters

  double fineRootFrac;  // fraction of wood carbon allocated to fine roots
  double coarseRootFrac;  // fraction of wood carbon that is coarse roots
  double fineRootAllocation;  // fraction of NPP allocated to fine roots
  double woodAllocation;  // fraction of NPP allocated to the roots
  double fineRootExudation;  // fraction of GPP exuded to the soil
  double coarseRootExudation;  // fraction of NPP exuded to the soil
  // Calculated param
  double coarseRootAllocation;  // fraction of NPP allocated to the coarse roots

  // 7-1=6 parameters

  double fineRootTurnoverRate;  // turnover of fine roots (per year rate)
  double coarseRootTurnoverRate;  // turnover of coarse roots (per year rate)
  double baseFineRootResp;  // base respiration rate of fine roots  (per year
                            // rate)
  double baseCoarseRootResp;  // base respiration rate of coarse roots (per year
                              // rate)
  double fineRootQ10;  // Q10 of fine roots
  double coarseRootQ10;  // Q10 of coarse roots
                         // 6 parameters

  double baseMicrobeResp;  // base respiration rate of microbes
  double microbeQ10;  // Q10 of coarse roots
  double microbePulseEff;  // fraction of exudates that microbes immediately
                           // use.
  // Total parameters=7+9+8+5+14+15+4+5+1+5+6+3=81

  // double soilBreakdownCoeff[NUMBER_SOIL_CARBON_POOLS];  // Rate coefficients
  // if we do not have a multipool quality model

} Params;

#define NUM_PARAMS (sizeof(Params) / sizeof(double))

// the state of the environment
typedef struct Environment {
  double plantWoodC;  // carbon in plant wood (above-ground + roots) (g C * m^-2
                      // ground area)
  double plantLeafC;  // carbon in leaves (g C * m^-2 ground area)
  double litter;  // carbon in litter (g C * m^-2 ground area)
#if SOIL_MULTIPOOL
  double soil[NUMBER_SOIL_CARBON_POOLS];  // if we have more than one quality
                                          // pool we use this
#else
  double soil;
#endif

  double litterWater;  // water in litter (evaporative) layer (cm)
  double soilWater;  // plant available soil water (cm)
  double snow;  // snow pack (cm water equiv.)

  double microbeC;  // carbon in microbes g C m-2 ground area

  double coarseRootC;
  double fineRootC;
} Envi;

// fluxes as per-day rates
typedef struct FluxVars {
  double photosynthesis;  // GROSS photosynthesis (g C * m^-2 ground area *
                          // day^-1)
  double leafCreation;  // g C * m^-2 ground area * day^-1 transferred from wood
                        // to leaves
  double leafLitter;  // g C * m^-2 ground area * day^-1 leaves falling
  double woodLitter;  // g C * m^-2 ground area * day^-1
  double rVeg;  // vegetation respiration (g C * m^-2 ground area * day^-1)
  double litterToSoil;  // g C * m^-2 ground area * day^-1 litter turned into
                        // soil
  double rLitter;  // g C * m^-2 ground area * day^-1 respired by litter
  double rSoil;  // g C * m^-2 ground area * day^-1 respired by soil
  double rain;  // cm water * day^-1 (only liquid precip.)
  double snowFall;  // cm water equiv. * day^-1
  double immedEvap;  // rain that's intercepted and immediately evaporated (cm
                     // water * day^-1)
  double snowMelt;  // cm water equiv. * day^-1
  double sublimation;  // cm water equiv. * day^-1
  double fastFlow;  // water entering soil that goes directly to drainage (out
                    // of system) (cm water * day^-1)
  double evaporation;  // evaporation from top of soil (cm water * day^-1)
  double topDrainage;  // drainage from top of soil to lower level (cm water *
                       // day^-1)
  double bottomDrainage;  // drainage from lower level of soil out of system (cm
                          // water * day^-1)
  double transpiration;  // cm water * day^-1
  double rWood;  // g C m^-2 ground area day^-1 of wood respiration
  double rLeaf;  // g C m^-2 ground area day^-1 of leaf respiration

#if SOIL_MULTIPOOL
  double maintRespiration[NUMBER_SOIL_CARBON_POOLS];  // Microbial maintenance
                                                      // respiration rate g C
                                                      // m-2 ground area day^-1
  double microbeIngestion[NUMBER_SOIL_CARBON_POOLS];
#else
  double maintRespiration;
  double microbeIngestion;
#endif

  double fineRootLoss;  // Loss rate of fine roots (turnover + exudation)
  double coarseRootLoss;  // Loss rate of coarse roots (turnover + exudation)

  double fineRootCreation;  // Creation rate of fine roots
  double coarseRootCreation;  // Creation rate of coarse roots
  double woodCreation;  // Creation rate of wood

  double rCoarseRoot;  // Coarse root respiration
  double rFineRoot;  // Fine root respiration

  double soilPulse;  // Exudates into the soil
} Fluxes;

typedef struct TrackerVars {  // variables to track various things
  double gpp;  // g C * m^-2 taken up in this time interval: GROSS
               // photosynthesis
  double rtot;  // g C * m^-2 respired in this time interval
  double ra;  // g C * m^-2 autotrophic resp. in this time interval
  double rh;  // g C * m^-2 heterotrophic resp. in this time interval
  double npp;  // g C * m^-2 taken up in this time interval
  double nee;  // g C * m^-2 given off in this time interval
  double yearlyGpp;  // g C * m^-2 taken up, year to date: GROSS photosynthesis
  double yearlyRtot;  // g C * m^-2 respired, year to date
  double yearlyRa;  // g C * m^-2 autotrophic resp., year to date
  double yearlyRh;  // g C * m^-2 heterotrophic resp., year to date
  double yearlyNpp;  // g C * m^-2 taken up, year to date
  double yearlyNee;  // g C * m^-2 given off, year to date
  double totGpp;  // g C * m^-2 taken up, to date: GROSS photosynthesis
  double totRtot;  // g C * m^-2 respired, to date
  double totRa;  // g C * m^-2 autotrophic resp., to date
  double totRh;  // g C * m^-2 heterotrophic resp., to date
  double totNpp;  // g C * m^-2 taken up, to date
  double totNee;  // g C * m^-2 given off, to date
  double evapotranspiration;  // cm water evaporated/sublimated (sublimed???) or
                              // transpired in this time step
  double soilWetnessFrac; /* mean fractional soil wetness (soilWater/soilWHC)
           over this time step (linear mean: mean of wetness at start of time
           step and wetness at end of time step) */
  double fa;  // g C * m^-2 of net photosynthesis (GPP - leaf respiration) in
              // this time interval
  double fr;  // g C * m^-2 of non foliar respiration (soil + wood respiration)
              // in this time interval
  double totSoilC;  // total soil carbon across all the pools

  double rRoot;  // g C m-2 of root respiration
  double rSoil;  // Soil respiration (microbes+root)

  double rAboveground;  // Wood and foliar respiration
  double fpar;  // 8 day mean fractional photosynthetically active radiation
                // (percentage)
  double plantWoodC;  // carbon in plant wood (above-ground + roots) (g C * m^-2
                      // ground area)
  double LAI;  // Leaf Area Index - leaf area per ground area / divide
               // PlantLeafC by leafCSpWt
  double yearlyLitter;  // g C * m^-2 litterfall, year to date: SUM litter
} Trackers;

typedef struct PhenologyTrackersStruct { /* variables to track each year's
          phenological development. Only used in leafFluxes function, but made
          global so can be initialized dynamically, based on day of year of
          first climate record */
  int didLeafGrowth;  // have we done leaf growth at start of growing season
                      // this year? (0 = no, 1 = yes)
  int didLeafFall;  // have we done leaf fall at end of growing season this
                    // year? (0 = no, 1 = yes)
  int lastYear;  // year of previous time step, for tracking when we hit a new
                 // calendar year
} PhenologyTrackers;

// global variables:
// made global so they can be initialized in a separate function
// so they only have to be initialized once

static ClimateNode *firstClimate;  // pointers to first climate

// more global variables:
// these are global to increase efficiency (avoid lots of parameter passing)

static Params params;
static Envi envi;  // state variables
static Trackers trackers;
static PhenologyTrackers phenologyTrackers;
static MeanTracker *meanNPP;  // running mean of NPP over some fixed time
                              // (stored in g C * m^-2 * day^-1)
static MeanTracker *meanGPP;  // running mean of GPP over some fixed time for
                              // linkages (stored in g C * m^-2 * day^-1)
static MeanTracker *meanFPAR;  // running mean of FPAR of some fixed time for
                               // MODIS

static ClimateNode *climate;  // current climate
static Fluxes fluxes;

#if EVENT_HANDLER
static EventNode *events;
static EventNode *event;
static FILE *eventOutFile;
#endif

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

#if GDD
  double thisGdd;  // growing degree days of this time step
  double gdd = 0.0;  // growing degree days since the last Jan. 1
#endif

  int status;  // status of the read

  // for format check
  int firstLoc, dummyLoc, numFields;
  int expectedNumCols = NUM_CLIM_FILE_COLS;
  char *firstLine = NULL;
  size_t lineCap = 0;
  int hasLoc = 0;
  const char *SEPARATORS = " \t\n\r";  // characters that can separate values in
                                       // parameter files

  in = openFile(climFile, "r");

  // Check format of first line to see if location is still specified (we will
  // ignore it if so)
  if (getline(&firstLine, &lineCap, in) == -1) {  // EOF
    printf("Error: no climate data in %s\n", climFile);
    exit(EXIT_CODE_INPUT_FILE_ERROR);
  }

  numFields = countFields(firstLine, SEPARATORS);
  if (numFields == expectedNumCols + 1) {
    printf("WARNING: ignoring location column in %s (found %d cols)\n",
           climFile, numFields);
    ++expectedNumCols;
    hasLoc = 1;
  }

  if (hasLoc) {
    status = sscanf(firstLine,  // NOLINT
                    "%d %d %d %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf",
                    &firstLoc, &year, &day, &time, &length, &tair, &tsoil, &par,
                    &precip, &vpd, &vpdSoil, &vPress, &wspd, &soilWetness);
  } else {
    status = sscanf(firstLine,  // NOLINT
                    "%d %d %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf", &year,
                    &day, &time, &length, &tair, &tsoil, &par, &precip, &vpd,
                    &vpdSoil, &vPress, &wspd, &soilWetness);
  }
  free(firstLine);

  if (status != expectedNumCols) {
    printf("Error reading climate file: bad data on first line\n");
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
    curr->soilWetness = soilWetness;

#if GDD
    if (year != lastYear) {  // HAPPY NEW YEAR!
      gdd = 0;  // reset growing degree days
    }
    thisGdd = tair * length;
    if (thisGdd < 0) {  // can't have negative growing degree days
      thisGdd = 0;
    }
    gdd += thisGdd;
    curr->gdd = gdd;
#endif

    lastYear = year;

    if (hasLoc) {
      status =
          fscanf(in,  // NOLINT
                 "%d %d %d %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf",
                 &dummyLoc, &year, &day, &time, &length, &tair, &tsoil, &par,
                 &precip, &vpd, &vpdSoil, &vPress, &wspd, &soilWetness);
    } else {
      status = fscanf(in,  // NOLINT
                      "%d %d %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf",
                      &year, &day, &time, &length, &tair, &tsoil, &par, &precip,
                      &vpd, &vpdSoil, &vPress, &wspd, &soilWetness);
    }
    if (status != EOF) {
      if (status != expectedNumCols) {
        printf("Error reading climate file: bad data near year %d day %d\n",
               year, day);
        exit(EXIT_CODE_INPUT_FILE_ERROR);
      }
      // Check for older file with multiple locations - that's an error now
      if (dummyLoc != firstLoc) {
        printf("Error reading climate file %s: multiple locations not "
               "supported\n",
               climFile);
        exit(EXIT_CODE_INPUT_FILE_ERROR);
      }

      // still reading climate records from the same place: add a node at end
      // of linked list
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
  ModelParams *modelParams;  // to prevent lots of
                             // unnecessary dereferences
  paramF = openFile(paramFile, "r");

  *modelParamsPtr = newModelParams(NUM_PARAMS);
  modelParams = *modelParamsPtr;  // to prevent lots of unnecessary dereferences

  // clang-format off
  // NOLINTBEGIN
  initializeOneModelParam(modelParams, "plantWoodInit", &(params.plantWoodInit), 1);
  initializeOneModelParam(modelParams, "laiInit", &(params.laiInit), 1);
  initializeOneModelParam(modelParams, "litterInit", &(params.litterInit), 1);
  initializeOneModelParam(modelParams, "soilInit", &(params.soilInit), 1);
  initializeOneModelParam(modelParams, "litterWFracInit", &(params.litterWFracInit), 1);
  initializeOneModelParam(modelParams, "soilWFracInit", &(params.soilWFracInit), 1);
  initializeOneModelParam(modelParams, "snowInit", &(params.snowInit), 1);
  initializeOneModelParam(modelParams, "aMax", &(params.aMax), 1);
  initializeOneModelParam(modelParams, "aMaxFrac", &(params.aMaxFrac), 1);
  initializeOneModelParam(modelParams, "baseFolRespFrac", &(params.baseFolRespFrac), 1);

  initializeOneModelParam(modelParams, "psnTMin", &(params.psnTMin), 1);
  initializeOneModelParam(modelParams, "psnTOpt", &(params.psnTOpt), 1);
  initializeOneModelParam(modelParams, "vegRespQ10", &(params.vegRespQ10), 1);
  initializeOneModelParam(modelParams, "growthRespFrac", &(params.growthRespFrac), GROWTH_RESP);
  initializeOneModelParam(modelParams, "frozenSoilFolREff", &(params.frozenSoilFolREff), 1);
  initializeOneModelParam(modelParams, "frozenSoilThreshold", &(params.frozenSoilThreshold), 1);
  initializeOneModelParam(modelParams, "dVpdSlope", &(params.dVpdSlope), 1);
  initializeOneModelParam(modelParams, "dVpdExp", &(params.dVpdExp), 1);
  initializeOneModelParam(modelParams, "halfSatPar", &(params.halfSatPar), 1);
  initializeOneModelParam(modelParams, "attenuation", &(params.attenuation), 1);

  initializeOneModelParam(modelParams, "leafOnDay", &(params.leafOnDay), !((GDD) || (SOIL_PHENOL)));
  initializeOneModelParam(modelParams, "gddLeafOn", &(params.gddLeafOn), GDD);
  initializeOneModelParam(modelParams, "soilTempLeafOn", &(params.soilTempLeafOn), SOIL_PHENOL);
  initializeOneModelParam(modelParams, "leafOffDay", &(params.leafOffDay), 1);
  initializeOneModelParam(modelParams, "leafGrowth", &(params.leafGrowth), 1);
  initializeOneModelParam(modelParams, "fracLeafFall", &(params.fracLeafFall), 1);
  initializeOneModelParam(modelParams, "leafAllocation", &(params.leafAllocation), 1);
  initializeOneModelParam(modelParams, "leafTurnoverRate", &(params.leafTurnoverRate), 1);
  initializeOneModelParam(modelParams, "baseVegResp", &(params.baseVegResp), 1);
  initializeOneModelParam(modelParams, "litterBreakdownRate", &(params.litterBreakdownRate), LITTER_POOL);

  initializeOneModelParam(modelParams, "fracLitterRespired", &(params.fracLitterRespired), LITTER_POOL);
  initializeOneModelParam(modelParams, "baseSoilResp", &(params.baseSoilResp), 1);
  initializeOneModelParam(modelParams, "baseSoilRespCold", &(params.baseSoilRespCold), SEASONAL_R_SOIL);
  initializeOneModelParam(modelParams, "soilRespQ10", &(params.soilRespQ10), 1);
  initializeOneModelParam(modelParams, "soilRespQ10Cold", &(params.soilRespQ10Cold), SEASONAL_R_SOIL);
  initializeOneModelParam(modelParams, "coldSoilThreshold", &(params.coldSoilThreshold), SEASONAL_R_SOIL);

  initializeOneModelParam(modelParams, "E0", &(params.E0), LLOYD_TAYLOR);
  initializeOneModelParam(modelParams, "T0", &(params.T0), LLOYD_TAYLOR);
  initializeOneModelParam(modelParams, "soilRespMoistEffect", &(params.soilRespMoistEffect), ((WATER_HRESP) && !(DAYCENT_WATER_HRESP)));
  initializeOneModelParam(modelParams, "waterRemoveFrac", &(params.waterRemoveFrac), 1);
  initializeOneModelParam(modelParams, "frozenSoilEff", &(params.frozenSoilEff), 1);
  initializeOneModelParam(modelParams, "wueConst", &(params.wueConst), 1);
  initializeOneModelParam(modelParams, "litterWHC", &(params.litterWHC), 1);
  initializeOneModelParam(modelParams, "soilWHC", &(params.soilWHC), 1);
  initializeOneModelParam(modelParams, "immedEvapFrac", &(params.immedEvapFrac), COMPLEX_WATER);
  initializeOneModelParam(modelParams, "fastFlowFrac", &(params.fastFlowFrac), COMPLEX_WATER);
  initializeOneModelParam(modelParams, "leafPoolDepth", &(params.leafPoolDepth), LEAF_WATER);

  initializeOneModelParam(modelParams, "snowMelt", &(params.snowMelt), SNOW);
  initializeOneModelParam(modelParams, "litWaterDrainRate", &(params.litWaterDrainRate), LITTER_WATER_DRAINAGE);
  initializeOneModelParam(modelParams, "rdConst", &(params.rdConst), (COMPLEX_WATER) || (PENMAN_MONTEITH_TRANS));
  initializeOneModelParam(modelParams, "rSoilConst1", &(params.rSoilConst1), COMPLEX_WATER);
  initializeOneModelParam(modelParams, "rSoilConst2", &(params.rSoilConst2), COMPLEX_WATER);
  initializeOneModelParam(modelParams, "leafCSpWt", &(params.leafCSpWt), 1);
  initializeOneModelParam(modelParams, "cFracLeaf", &(params.cFracLeaf), 1);
  initializeOneModelParam(modelParams, "woodTurnoverRate", &(params.woodTurnoverRate), 1);
  initializeOneModelParam(modelParams, "qualityLeaf", &(params.qualityLeaf), SOIL_QUALITY);
  initializeOneModelParam(modelParams, "qualityWood", &(params.qualityWood), SOIL_QUALITY);

  initializeOneModelParam(modelParams, "efficiency", &(params.efficiency), (SOIL_QUALITY) || (MICROBES));
  initializeOneModelParam(modelParams, "maxIngestionRate", &(params.maxIngestionRate), (SOIL_QUALITY) || (MICROBES));
  initializeOneModelParam(modelParams, "halfSatIngestion", &(params.halfSatIngestion), MICROBES);
  initializeOneModelParam(modelParams, "totNitrogen", &(params.totNitrogen), STOICHIOMETRY);
  initializeOneModelParam(modelParams, "microbeNC", &(params.microbeNC), STOICHIOMETRY);
  initializeOneModelParam(modelParams, "microbeInit", &(params.microbeInit), (SOIL_QUALITY) || (MICROBES));
  initializeOneModelParam(modelParams, "fineRootFrac", &(params.fineRootFrac), ROOTS);
  initializeOneModelParam(modelParams, "coarseRootFrac", &(params.coarseRootFrac), ROOTS);

  initializeOneModelParam(modelParams, "fineRootAllocation", &(params.fineRootAllocation), ROOTS);
  initializeOneModelParam(modelParams, "woodAllocation", &(params.woodAllocation), ROOTS);

  initializeOneModelParam(modelParams, "fineRootExudation", &(params.fineRootExudation), ROOTS);
  initializeOneModelParam(modelParams, "coarseRootExudation", &(params.coarseRootExudation), ROOTS);
  initializeOneModelParam(modelParams, "fineRootTurnoverRate", &(params.fineRootTurnoverRate), ROOTS);
  initializeOneModelParam(modelParams, "coarseRootTurnoverRate", &(params.coarseRootTurnoverRate), ROOTS);
  initializeOneModelParam(modelParams, "baseFineRootResp", &(params.baseFineRootResp), ROOTS);
  initializeOneModelParam(modelParams, "baseCoarseRootResp", &(params.baseCoarseRootResp), ROOTS);
  initializeOneModelParam(modelParams, "fineRootQ10", &(params.fineRootQ10), ROOTS);
  initializeOneModelParam(modelParams, "coarseRootQ10", &(params.coarseRootQ10), ROOTS);

  initializeOneModelParam(modelParams, "baseMicrobeResp", &(params.baseMicrobeResp), MICROBES);
  initializeOneModelParam(modelParams, "microbeQ10", &(params.microbeQ10), MICROBES);
  initializeOneModelParam(modelParams, "microbePulseEff", &(params.microbePulseEff), (ROOTS) && (MICROBES) );
  initializeOneModelParam(modelParams, "m_ballBerry", &(params.m_ballBerry), 1);
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
  fprintf(out, "year day time plantWoodC plantLeafC ");

#if SOIL_MULTIPOOL
  int counter;

  for (counter = 0; counter < NUMBER_SOIL_CARBON_POOLS; counter++) {
    fprintf(out, "soil(%8.2f) ", envi.soil[counter]);
  }
  fprintf(out, "totSoilC ");

#else
  fprintf(out, "soil ");
#endif

  fprintf(out, "microbeC ");
  fprintf(out, "coarseRootC fineRootC ");
  fprintf(out, "litter litterWater soilWater soilWetnessFrac snow ");
  fprintf(out, "npp nee cumNEE gpp rAboveground rSoil rRoot ra rh rtot "
               "evapotranspiration fluxestranspiration fPAR\n");
}

/*!
 * Print current state values to output file
 * @param out File pointer for output
 * @param year
 * @param day
 * @param time
 */
void outputState(FILE *out, int year, int day, double time) {

  fprintf(out, "%4d %3d %5.2f %8.2f %8.2f ", year, day, time, envi.plantWoodC,
          envi.plantLeafC);

#if SOIL_MULTIPOOL
  int counter;

  for (counter = 0; counter < NUMBER_SOIL_CARBON_POOLS; counter++) {
    fprintf(out, "%8.2f ", envi.soil[counter]);
  }
  fprintf(out, "%8.2f ", trackers.totSoilC);

#else
  fprintf(out, "%8.2f ", envi.soil);
#endif

  fprintf(out, "%8.2f ", envi.microbeC);

  fprintf(out, "%8.2f %8.2f", envi.coarseRootC, envi.fineRootC);

  fprintf(out, " %8.2f %8.3f %8.2f %8.3f %8.2f ", envi.litter, envi.litterWater,
          envi.soilWater, trackers.soilWetnessFrac, envi.snow);
  fprintf(out,
          "%8.2f %8.2f %8.2f %8.2f %8.3f %8.3f %8.3f %8.3f %8.3f %8.3f %8.8f "
          "%8.4f %8.4f\n",
          trackers.npp, trackers.nee, trackers.totNee, trackers.gpp,
          trackers.rAboveground, trackers.rSoil, trackers.rRoot, trackers.ra,
          trackers.rh, trackers.rtot, trackers.evapotranspiration,
          fluxes.transpiration, trackers.fpar);

  // note without modeling root dynamics

  // trackers.fa, trackers.fr,
  // fluxes.rLeaf*climate->length,trackers.evapotranspiration
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

#if EVENT_HANDLER
// de-allocate space used for events linked list
void freeEventList() {
  EventNode *curr, *prev;

  curr = events;
  while (curr != NULL) {
    prev = curr;
    curr = curr->nextEvent;
    free(prev);
  }
}
#endif

// !!! functions for calculating auxiliary variables !!!

// rather than returning a value, they have as parameters the variable(s) which
// they modify so a single function can modify multiple variables

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

  static const int NUM_LAYERS = 6;
  // believe it or not, 6 layers gives approximately the same result as 100
  // layers

  int layer;  // counter
  double cumLai;  // lai from this layer up
  double lightIntensity;
  double currLightEff = 0.0, cumLightEff;
  double currfAPAR = 0.0, cumfAPAR;
  int coeff;  // current coefficient in Simpson's rule

  if (lai > 0 && par > 0) {  // must have at least some leaves and some light
    cumLightEff = 0.0;  // the running sum
    layer = 0;
    coeff = 1;
    int err;
    double fAPAR;  // Calculation of fAPAR according to 1 - exp(attenuation*LAI)
    // double APAR;    // Absorbed PAR by the canopy
    // double lightIntensityTop, lightIntensityBottom;  // PAR absorbed by the
    // canopy
    cumfAPAR = 0.0;

    while (layer <= NUM_LAYERS) {
      cumLai = lai * ((double)layer / NUM_LAYERS);  // lai from this layer up
                                                    // (starting at top)

      // between 0 and par
      lightIntensity = par * exp(-1.0 * params.attenuation * cumLai);

      // between 0 and 1
      currLightEff = (1 - pow(2, (-1.0 * lightIntensity / params.halfSatPar)));

      // when lightIntensity = halfSatPar, currLightEff = 1/2
      cumLightEff += coeff * currLightEff;

      currfAPAR = 1 - (lightIntensity / par);
      cumfAPAR += coeff * currfAPAR;

      // now move to the next layer:
      layer++;
      coeff = 2 * (1 + layer % 2);  // coeff. goes 1, 4, 2, 4, ..., 2, 4, 2
    }
    // last value should have had a coefficient of 1, but actually had a
    // coefficient of 2, so subtract 1:
    cumLightEff -= currLightEff;
    *lightEff = cumLightEff / (3.0 * NUM_LAYERS);  // multiplying by (h/3) in
                                                   // Simpson's rule

    // now calculate fAPAR
    cumfAPAR -= currfAPAR;
    // the average value, multiplying by h/3 in Simpson's rule, and dividing by
    // the number of steps
    fAPAR = cumfAPAR / (3.0 * NUM_LAYERS);

    // lightIntensityTop = par;  // Energy at the top of the canopy
    // lightIntensityBottom = par * exp(-1.0 * params.attenuation * lai); // LAI
    // at the bottom of the canopy
    //  between 0 and par
    // this is the amount of incident par
    // APAR = params.m_ballBerry * (lightIntensityTop - lightIntensityBottom);
    // // APAR at this layer
    //  is a fraction of the difference between incoming par and transmitted par

    // fAPAR = APAR / par;  // Take the average across all of the layers
    // fAPAR =  (1 - exp(-1.0 * params.attenuation * lai));  // 4/28/11: Update
    // from TQuaife

    err = addValueToMeanTracker(meanFPAR, fAPAR, 1);  // update running mean of
                                                      // FPAR (we don't care
                                                      // about climate length)
    if (err != 0) {
      printf("******* Error type %d while trying to add value to FPAR mean "
             "tracker in sipnet:potPSN() *******\n",
             err);
      printf("FPAR = %f, climate->length = %f\n", fAPAR, climate->length);
      printf("Suggestion: try changing MEAN_FPAR_MAX_ENTRIES in sipnet.c\n");
      exit(1);
    }

  } else {  // no leaves or no light!
    *lightEff = 0;
  }
}

// calculate gross photosynthesis without water effect (g C * m^-2 ground area *
// day^-1) and base foliar respiration without temp, water, etc. (g C * m^-2
// ground area * day^-1)
void potPsn(double *potGrossPsn, double *baseFolResp, double lai, double tair,
            double vpd, double par, int /*day*/) {
  double grossAMax;  // maximum possible gross respiration (nmol CO2 * g^-1 leaf
                     // * sec^-1)
  double dTemp, dVpd, lightEff;  // decrease in photosynth. due to temp, vpd and
                                 // amt. of light absorbed
  double respPerGram;  // base foliar respiration in nmol CO2 * g^-1 leaf *
                       // sec^-1
  double conversion; /* convert from (nmol CO2 * g^-1 leaf * sec^-1)
           to (g C * m^-2 ground area * day^-1) */

  respPerGram = params.baseFolRespFrac * params.aMax;
  // foliar respiration, unmodified by temp, etc.
  grossAMax = params.aMax * params.aMaxFrac + respPerGram;

  dTemp = (params.psnTMax - tair) * (tair - params.psnTMin) /
          pow((params.psnTMax - params.psnTMin) / 2.0, 2);
  if (dTemp < 0) {
    dTemp = 0.0;
  }
  dVpd = 1.0 - params.dVpdSlope * pow(vpd, params.dVpdExp);
  if (dVpd < 0) {
    dVpd = 0.0;
  }
  calcLightEff(&lightEff, lai, par);

  conversion = C_WEIGHT * (1.0 / TEN_9) *
               (params.leafCSpWt / params.cFracLeaf) * lai *
               SEC_PER_DAY;  // to convert units
  *potGrossPsn = grossAMax * dTemp * dVpd * lightEff * conversion;
  *baseFolResp = respPerGram * conversion;  // do foliar resp. even if no
                                            // photosynthesis in this time step
}

void moisture_bwb(double *trans, double *dWater, double potGrossPsn, double vpd,
                  double vPress, double plantLeafC, double leafCSpWt,
                  double soilWater) {
  /*moisture_bwb:  Calculates moisture use by using a Ball Woodrow Berry
   * Instead of A (net photosynthesis) we are driving the Ball Woodrow Berry
   * equation with potGrossPsn units: g C * m^-2 ground area * day^-1
   * formulation which estimates gs from A */
  double potTrans;  // potential transpiration in the absense of plant water
                    // stress (cm H20 * day^-1)
  double removableWater;
  double gs_canopy;  // gs stomatal conductance... canopy level
  double CO2_stom = 380;
  double vPress_sat;  // Saturating Vapor Press (sum of VPD and vPress)
  double RH_pcent;  // Relative Humidity - the ratio of vPress to saturating
                    // vapor pressure
  double lai_int;  // calculate lai from current plantLeafC and the leafCSpwt */
  lai_int = envi.plantLeafC / params.leafCSpWt;
  /*  We do not require lai since potGrossPsn is in units of
   *  g C * m^-2 ground area * day^-1 */
  vPress_sat = climate->vPress + climate->vpd;  // calculate saturating vapor
                                                // pressure
  RH_pcent = climate->vPress / vPress_sat;  // calculate relative humidity for
                                            // ball berry

  //  double wue; // water use efficiency, in mg CO2 fixed * g^-1 H20 transpired

  if (potGrossPsn < TINY) {  // avoid divide by 0
    *trans = 0.0;  // no photosynthesis -> no transpiration
    *dWater = 1;  // dWater doesn't matter, since we don't have any
                  // photosynthesis
  } else {
    /*Ball Berry Equation
     * unit issues:
     * potGrossPsn should be converted to
     * micro mol c02 per m^2 leaf area
     * there are 1/12 mol of carbon in a gram
     * potGrossPsn/12*1000*LAI = umol carbon dioxide per
     * m^2
     * */

    gs_canopy = params.m_ballBerry * potGrossPsn / 12 * 1000 * lai_int *
                (RH_pcent / CO2_stom);

    // gcan = m*A*RelHum/CO2;
    // mol  / m^2 leaf area

    potTrans = (gs_canopy * climate->vpd) / lai_int * 20 / 1000;
    /* gs_canopy*climate_>vpd is the transpiration rate mol/m^2 leaf area per
     * day dividing by lai_init converts to mol per m^2 ground area there are
     * 20g water per mol multiply by the density of water 1cm^3/g multipy by
     * 100cm and divide by 10^6cm^3
     *
     *  must convert to cm3 per cm2 land area.
     */
    removableWater = soilWater * params.waterRemoveFrac;
    if (climate->tsoil < params.frozenSoilThreshold) {
      // frozen soil - less or no water available
      /* frozen soil effect: fraction of water available if soil is frozen
       * (assume amt. of water avail. w/ frozen soil scales linearly with amt.
       * of water avail. in thawed soil) */
      removableWater *= params.frozenSoilEff;
    }
    if (removableWater >= potTrans) {
      *trans = potTrans;
    } else {
      *trans = removableWater;
    }

#if WATER_PSN  // we're modeling water stress
    *dWater = *trans / potTrans;  // from PnET: equivalent to setting DWATER_MAX
                                  // = 1
#else  // WATER_PSN = 0
    if (climate->tsoil < params.frozenSoilThreshold &&
        params.frozenSoilEff == 0)
      // (note: can't have partial shutdown of psn with frozen soil if WATER_PSN
      // = 0)
      *dWater = 0;  // still allow total shut down of psn. if soil is frozen
    else  // either soil is thawed, or frozenSoilEff > 0
      *dWater = 1;  // no water stress, even if *trans/potTrans < 1
#endif  // WATER_PSN
    // printf("Remove %f potT %f dW %f vpd %f \n", removableWater, potTrans,
    // *dWater, climate->vpd);
  }
}

/* it would be preferable to include CO2 concentration as an input variable in
 * the climate file
 *
 *  Ball Berry Equation or Ball-Woodrow-Berry model
 * gsc = gsc0 + k(A)(Hs/Ccs)
 * where gsc is stomatal conductance,  gsc0 is a basal conductance value which
 * we assume is =0 k is an assumed splope parameter estimated in our case from
 * the literature, Hs is the relative humidity - calculated here as
 * (vPress/(vPress+vpd)), Ccs is the CO2 concentration at the stomata; currently
 * we will assume this is 370 and I may edit the Climate input file to include
 * CO2 in future. BALL_BERRY_m is hardcoded into the equation at 3.89  */

// printf("%d\n", gs_canopy);

// Penman Monteith method of estimating transiration and water use
//  Started Nov 2006 - coding commenced Nov 20th

void moisture_pm(double *trans, double *dWater, double potGrossPsn, double vpd,
                 double soilWater) {
  double potTrans;  // potential transpiration in the absense of plant water
                    // stress (cm H20 * day^-1)
  double removableWater;
  // not used   double wue; // water use efficiency, in mg CO2 fixed * g^-1 H20
  // transpired
  double gapfraction = 17;  // gap fraction = 17% - the Penman Monteith Equation
                            // doesn't appear to be sensitive to gapfraction;
  double rCanConst = 30.8;  // Rcan = rCanConst/windspeed - 30.8 ;
  // is the median rCanConst to achieve the measured Rcan of 8.5 s/m;
  double DELTA;  // Delta is calculated as d Vsat/ d Tair;

  /*
   * required constants and other values
   * Rn // Net radiation // read in from clim file
   * G // soil heat flux // read in from clim file
   * RHO // DENSITY OF (DRY) AIR (KG M-3)
   * CP // SPECIFIC HEAT OF AIR AT CONST PRESSURE (J KG-1 K-1)
   * VPD // VAPOR PRESSURE DEFICIT (PA)
   * rb - BULK AERODYNAMIC RESISTANCE (S/M)
   * rc - CANOPY AERODYNAMIC RESISTANCE (S/M)
   * LAMDA //LATENT HEAT OF VAPORIZATION OF WATER (J Kg-1) - 2450 @20C
   * GAMMA // PSYCHROMETRIC CONSTANT Kg M-2 S
   * DELTA slope of the saturation vapour pressure vs Temperature relationship
   */
  if (potGrossPsn < TINY) {  // avoid divide by 0
    *trans = 0.0;  // no photosynthesis -> no transpiration
    *dWater = 1;  // dWater doesn't matter, since we don't have any
                  // photosynthesis
  } else {
    DELTA = (2508.3 / ((climate->tair + 237.3) * (climate->tair + 237.3)) *
             exp((17.3 * climate->tair) / (climate->tair + 237.3)));
    potTrans =
        (DELTA * ((1 - gapfraction / 100) * climate->par - climate->tsoil) +
         (RHO * CP * vpd) / (rCanConst / climate->wspd)) /
        (DELTA + GAMMA * (1 + (params.rdConst / climate->wspd) /
                                  (rCanConst / climate->wspd)));

    /*
     * the aerodynamic resistance - of the canopy can be calculated as follows:
     * rc = [ln((Zm-d)/Zom)*ln((Zh-d)/Zoh)] / k*k*wspd ;
     *
     * rc aerodynamic resistance [s m-1],
     * Zm height of wind measurements [m],
     * Zh height of humidity measurements [m],
     * d zero plane displacement height [m],
     * Zom roughness length governing momentum transfer [m],
     * Zoh roughness length governing transfer of heat and vapour [m],
     * k von Karman's constant, 0.41 [-],
     * wspd wind speed at height z [m s-1].
     */
    removableWater = soilWater * params.waterRemoveFrac;
    if (climate->tsoil < params.frozenSoilThreshold) {
      // frozen soil - less or no water available
      removableWater *= params.frozenSoilEff;
    }
    /* frozen soil effect: fraction of water available if soil is frozen (assume
     * amt. of water avail. w/ frozen soil scales linearly with amt. of water
     * avail. in thawed soil) */
    if (removableWater >= potTrans) {
      *trans = potTrans;
    } else {
      *trans = removableWater;
    }

#if WATER_PSN  // we're modeling water stress
    *dWater = *trans / potTrans;  // from PnET: equivalent to setting DWATER_MAX
                                  // = 1
#else  // WATER_PSN = 0
    if (climate->tsoil < params.frozenSoilThreshold &&
        params.frozenSoilEff == 0)
      // (note: can't have partial shutdown of psn with frozen soil if WATER_PSN
      // = 0)
      *dWater = 0;  // still allow total shut down of psn. if soil is frozen
    else  // either soil is thawed, or frozenSoilEff > 0
      *dWater = 1;  // no water stress, even if *trans/potTrans < 1
#endif  // WATER_PSN
    // printf("PotGrossPsn: %f dWater %f potTrans %f\n", potGrossPsn, *dWater,
    // potTrans);
  }

  // printf("%f\n", *dWater);
}

// calculate transpiration (cm H20 * day^-1)
// and dWater (factor between 0 and 1)
void moisture(double *trans, double *dWater, double potGrossPsn, double vpd,
              double soilWater) {
  double potTrans;  // potential transpiration in the absense of plant water
                    // stress (cm H20 * day^-1)
  double removableWater;
  double wue;  // water use efficiency, in mg CO2 fixed * g^-1 H20 transpired

  if (potGrossPsn < TINY) {  // avoid divide by 0
    *trans = 0.0;  // no photosynthesis -> no transpiration
    *dWater = 1;  // dWater doesn't matter, since we don't have any
                  // photosynthesis
  }

  else {
    wue = params.wueConst / vpd;
    potTrans = potGrossPsn / wue * 1000.0 * (44.0 / 12.0) * (1.0 / 10000.0);
    // 1000 converts g to mg; 44/12 converts g C to g CO2, 1/10000 converts m^2
    // to cm^2

    removableWater = soilWater * params.waterRemoveFrac;
    if (climate->tsoil < params.frozenSoilThreshold) {
      // frozen soil - less or no water available
      /* frozen soil effect: fraction of water available if soil is frozen
                                               (assume amt. of water avail. w/
         frozen soil scales linearly with amt. of water avail. in thawed soil)
       */
      removableWater *= params.frozenSoilEff;
    }
    if (removableWater >= potTrans) {
      *trans = potTrans;
    } else {
      *trans = removableWater;
    }

#if WATER_PSN  // we're modeling water stress
    *dWater = *trans / potTrans;  // from PnET: equivalent to setting DWATER_MAX
                                  // = 1
#else  // WATER_PSN = 0
    if (climate->tsoil < params.frozenSoilThreshold &&
        params.frozenSoilEff == 0)
      // (note: can't have partial shutdown of psn with frozen soil if WATER_PSN
      // = 0)
      *dWater = 0;  // still allow total shut down of psn. if soil is frozen
    else  // either soil is thawed, or frozenSoilEff > 0
      *dWater = 1;  // no water stress, even if *trans/potTrans < 1
#endif  // WATER_PSN
  }

  // printf("%f\n", *dWater);
}

// have we passed the growing season-start leaf growth trigger this year?
// 0 = no, 1 = yes
// note: there may be some fluctuations in this signal for some methods of
// determining growing season start (e.g. for soil temp-based leaf growth)
int pastLeafGrowth(void) {

#if GDD
  // null pointer dereference warning suppressed on the next line
  return (climate->gdd >= params.gddLeafOn);  // NOLINT
#elif SOIL_PHENOL
  return (climate->tsoil >= params.soilTempLeafOn);  // soil temperature
                                                     // threshold
#else
  double currTime;
  int currYear;
  int currDay;
  currYear = climate->year;
  currDay = climate->day;
  currTime = (double)climate->day + climate->time / 24.0;

  // printf("stuff: %8d  %8d  \n",currYear,currDay);

  return (currTime >= params.leafOnDay);  // turn-on day
  // return 1;
#endif
}

// have we passed the growing season-end leaf fall trigger this year?
// 0 = no, 1 = yes
int pastLeafFall(void) {
  return ((climate->day + climate->time / 24.0) >=
          params.leafOffDay);  // turn-off
                               // day
  // return 1;
}

// calculate leafCreation and leafLitter fluxes (g C/m^2 ground/day)
// leafCreation is a fraction of recent mean npp, plus some constant amount at
// start of growing season leafLitter is a constant rate, plus some additional
// fraction of leaves at end of growing season
void leafFluxes(double *leafCreation, double *leafLitter, double plantLeafC) {
  double npp;  // temporal mean of recent npp (g C * m^-2 ground * day^-1)

  npp = getMeanTrackerMean(meanNPP);
  // first determine the fluxes that happen at every time step, not just start &
  // end of growing season:
  if (npp > 0) {
    *leafCreation = npp * params.leafAllocation;  // a fraction of NPP is
    // allocated to leaf growth
  } else {  // net loss of C in this time step - no C left for growth
    *leafCreation = 0;
  }
  // a constant fraction of leaves fall in each time step
  *leafLitter = plantLeafC * params.leafTurnoverRate;

  // now add add'l fluxes at start/end of growing season:

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

// calculate rain and snowfall (cm water equiv./day)
// calculate snow melt (cm water equiv./day) and drainage(cm/day)
// drainage here is any water that exceeds water holding capacity

// this is the simplified water flow function, which has only one soil moisture
// layer and does not do evaporation of any kind, or fast flow (sets these all
// to 0)
void simpleWaterFlow(double *rain, double *snowFall, double *immedEvap,
                     double *snowMelt, double *sublimation, double *fastFlow,
                     double *evaporation, double *topDrainage,
                     double *bottomDrainage, double water, double snow,
                     double precip, double temp, double length, double trans) {
  double netIn;  // net water into soil, in cm

#if SNOW  // we're modeling snow
  if (temp <= 0) {  // below freezing
    *snowFall = precip / length;
    *rain = 0;
    *snowMelt = 0;
  } else {  // above freezing
    *snowFall = 0;
    *rain = precip / length;
    if (snow > 0) {
      *snowMelt = params.snowMelt * temp;  // snow melt proportional to temp.
      if ((*snowMelt * length) > snow) {  // can only melt what's there!
        *snowMelt = snow / length;
      }
    } else {
      *snowMelt = 0;
    }
  }

#else  // not modeling snow
  *rain = precip / length;
  *snowFall = 0;
  *snowMelt = 0;
#endif  // #if snow

  netIn = (*rain + *snowMelt - trans) * length;
  *bottomDrainage = ((water + netIn) - params.soilWHC) / length;
  if (*bottomDrainage < 0) {
    *bottomDrainage = 0;
  }

  // all the things we don't model in simpleWaterFlow mode:
  *immedEvap = *sublimation = *fastFlow = *evaporation = *topDrainage = 0;
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

#if LEAF_WATER
  double maxLeafPool;

  maxLeafPool = lai * params.leafPoolDepth;  // calculate current leaf pool size
                                             // depending on lai
  *immedEvap = (*rain) * params.immedEvapFrac;

  // don't evaporate more than pool size, excess water will go to the soil
  if (*immedEvap > maxLeafPool)
    *immedEvap = maxLeafPool;

#else
  *immedEvap = (*rain) * params.immedEvapFrac;
#endif
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

// water calculations relating to the top (litter/evaporative) layer of soil
// (if only modeling one soil water layer, these relate to that layer)
// calculates fastFlow (cm/day), evaporation (cm/day) and drainage to lower
//   layer (cm/day)
// water is amount of water in evaporative layer (cm)
// whc is water holding capacity of evaporative layer (cm)
// (water and whc are used rather than envi variables to allow use of either
//   litterWater or soilWater)
// net rain (cm/day) is (rain - immedEvap) - i.e. the amount available to enter
//   the soil
// snowMelt in cm water equiv./day
// fluxesOut is the sum of any fluxes out of this layer that have already been
//   calculated
// (e.g. transpiration if we're just using one layer) (for calculating remaining
// water/drainage) (cm/day)
void evapSoilFluxes(double *fastFlow, double *evaporation, double *drainage,
                    double water, double whc, double netRain, double snowMelt,
                    double fluxesOut) {
  // conversion factor for evaporation
  static const double CONVERSION = (RHO * CP) / GAMMA * (1. / LAMBDA) * 1000. *
                                   1000. * (1. / 10000) * SEC_PER_DAY;
  // 1000 converts kg to g, 1000 converts kPa to Pa, 1/10000 converts m^2 to
  // cm^2

  double waterRemaining; /* keep running total of water remaining, in cm
          (to make sure we don't evap. or drain too much, and so we can drain
          any overflow) */
  double netIn;  // keep track of net water into soil, in cm/day
  double rd;  // aerodynamic resistance between ground and canopy air space
              // (sec/m)
  double rsoil;  // bare soil surface resistance (sec/m)

  netIn = netRain + snowMelt;

  // fast flow: fraction that goes directly to drainage
  *fastFlow = netIn * params.fastFlowFrac;
  netIn -= *fastFlow;

  // calculate evaporation:
  // first calculate how much water is left to evaporate (used later)
  waterRemaining =
      water + netIn * climate->length - fluxesOut * climate->length;

  // if there's a snow pack, don't evaporate from soil:
  if (envi.snow > 0) {
    *evaporation = 0;
  }

  // else no snow pack:
  else {
    rd = (params.rdConst) / (climate->wspd);  // aerodynamic resistance (sec/m)
    rsoil = exp(params.rSoilConst1 - params.rSoilConst2 * (water / whc));
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

#if LITTER_WATER_DRAINAGE  // we're calculating drainage even when evap. layer
                           // is not overflowing
  // drainage rate is proportional to fractional soil moisture
  *drainage = params.litWaterDrainRate * (water / whc);
  // make sure we don't drain more than we have:
  if (waterRemaining - (*drainage * climate->length) < TINY) {
    // leave a tiny little bit, to avoid negative water due to round-off errors
    *drainage = (waterRemaining - TINY) / climate->length;
    waterRemaining = 0;
  } else
    waterRemaining -= (*drainage * climate->length);
#else  // LITTER_WATER_DRAINAGE = 0
  *drainage = 0;
#endif

  // drain any water that remains beyond water holding capacity:
  if (waterRemaining > whc) {
    *drainage += (waterRemaining - whc) / (climate->length);
  }
}

// calculate drainage from bottom (soil/transpiration) layer (cm/day)
// based on current soil water store (cm), whc, drainage from top (cm/day) and
// transpiration (cm/day)
void transSoilDrainage(double *bottomDrainage, double topDrainage, double trans,
                       double soilWater) {
  double waterRemaining;  // cm

  waterRemaining = soilWater + (topDrainage - trans) * climate->length;
  *bottomDrainage = (waterRemaining - params.soilWHC) / (climate->length);
  if (*bottomDrainage < 0) {
    *bottomDrainage = 0;
  }
}

// calculates fastFlow (cm/day), evaporation (cm/day) and drainage to lower
// layer (cm/day)
// net rain (cm/day) is (rain - immedEvap) - i.e. the amount available to enter
//   the soil
// snowMelt in cm water equiv./day
// litterWater and soilWater in cm
// Also calculates drainage from bottom (soil/transpiration) layer (cm/day)
// Note that there may only be one layer, in which case we have only the
// bottomDrainage term, and evap. and trans. come from same layer.
void soilWaterFluxes(double *fastFlow, double *evaporation, double *topDrainage,
                     double *bottomDrainage, double netRain, double snowMelt,
                     double trans, double litterWater, double soilWater) {

#if LITTER_WATER
  evapSoilFluxes(fastFlow, evaporation, topDrainage, litterWater,
                 params.litterWHC, netRain, snowMelt, 0);
  // last parameter = fluxes out that have already been calculated = 0
  transSoilDrainage(bottomDrainage, *topDrainage, trans, soilWater);
#else  // only one soil moisture pool: evap. and trans. both happen from this
       // pool
  *topDrainage = 0;  // no top layer, only a bottom layer
  evapSoilFluxes(fastFlow, evaporation, bottomDrainage, soilWater,
                 params.soilWHC, netRain, snowMelt, trans);
  // last parameter = fluxes out that have already been calculated:
  // transpiration
#endif
}

// calculate GROSS phtosynthesis (g C * m^-2 * day^-1)
void getGpp(double *gpp, double potGrossPsn, double dWater) {
  *gpp = potGrossPsn * dWater;
}

// calculate foliar respiration and wood maint. resp, both in g C * m^-2 ground
// area * day^-1 does *not* explicitly model growth resp. (includes it in maint.
// resp)
void vegResp(double *folResp, double *woodResp, double baseFolResp) {
  *folResp = baseFolResp *
             pow(params.vegRespQ10, (climate->tair - params.psnTOpt) / 10.0);
  if (climate->tsoil < params.frozenSoilThreshold) {
    *folResp *= params.frozenSoilFolREff;  // allows foliar resp. to be shutdown
                                           // by a given fraction in winter
  }
  *woodResp = params.baseVegResp * envi.plantWoodC *
              pow(params.vegRespQ10, climate->tair / 10.0);
}

// calculate foliar respiration and wood maint. resp, both in g C * m^-2 ground
// area * day^-1 does *not* explicitly model growth resp. (includes it in maint.
// resp)
void calcRootResp(double *rootResp, double respQ10, double baseRate,
                  double poolSize) {
  *rootResp = baseRate * poolSize * pow(respQ10, climate->tsoil / 10.0);
}

// a second veg. resp. method:
// calculate foliar resp., wood maint. resp. and growth resp., all in g C * m^-2
// ground area * day^-1 growth resp. modeled in a very simple way
void vegResp2(double *folResp, double *woodResp, double *growthResp,
              double baseFolResp, double /*gpp*/) {
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

// Currently we have a water effect and an effect for different cold soil
// parameters  (this is maintenance respiration)
void calcMaintenanceRespiration(double tsoil, double water, double whc) {

  double moistEffect;
  double tempEffect;

#if DAYCENT_WATER_HRESP
  // calculate here so only have to do this ugly calculation once
  static const double daycentWaterExp = 3.22 * (1.7 - 0.55) / (0.55 + 0.007);
#endif

#if WATER_HRESP  // if soil moisture affects heterotrophic resp.

#if DAYCENT_WATER_HRESP  // using DAYCENT formulation
  // NOTE: would probably be best to make all these constants parameters that
  // are read in, but for now we have them hard-coded
  moistEffect = pow(((water / whc - 1.7) / (0.55 - 1.7)), daycentWaterExp) *
                pow((water / whc + 0.007) / (0.55 + 0.007), 3.22);
#else  // using PnET formulation
  moistEffect = pow((water / whc), params.soilRespMoistEffect);
#endif  // DAYCENT_WATER_HRESP

  if (climate->tsoil < 0) {
    moistEffect = 1;  // Ignore moisture effects in frozen soils
  }
#else  // no WATER_HRESP
  moistEffect = 1;
#endif  // WATER_HRESP

#if SOIL_MULTIPOOL
  int counter;

  double poolBaseRespiration, poolQ10;

  double soilQuality;

  // Loop through all the soil carbon pools
  for (counter = 0; counter < NUMBER_SOIL_CARBON_POOLS; counter++) {

#if SOIL_QUALITY
    // Ensure that the quality will never be zero
    soilQuality = (counter + 1) / NUMBER_SOIL_CARBON_POOLS;
#else
    soilQuality = 0;
#endif

#if SEASONAL_R_SOIL  // decide which parameters to use based on tsoil
    if (tsoil >= params.coldSoilThreshold) {  // use normal (warm temp.) params

      poolBaseRespiration = params.baseSoilResp;
      poolQ10 = params.soilRespQ10;

      tempEffect = poolBaseRespiration * pow(poolQ10, tsoil / 10);

    } else {  // use cold temp. params

      poolBaseRespiration = params.baseSoilRespCold;
      poolQ10 = params.soilRespQ10Cold;

      tempEffect = poolBaseRespiration * pow(poolQ10, tsoil / 10);
    }
#else  // SEASONAL_R_SOIL FALSE -> always use normal params

    poolBaseRespiration = params.baseSoilResp;
    poolQ10 = params.soilRespQ10;

    tempEffect = poolBaseRespiration * pow(poolQ10, tsoil / 10);

#endif

    fluxes.maintRespiration[counter] =
        envi.soil[counter] * moistEffect * tempEffect;
  }

#else  // We use a single pool model

#if MICROBES  // If we don't have a multipool approach, respiration is
              // determined by microbe biomass

  tempEffect = params.baseMicrobeResp * pow(params.microbeQ10, tsoil / 10);

  fluxes.maintRespiration = envi.microbeC * moistEffect * tempEffect;
#else

#if SEASONAL_R_SOIL  // decide which parameters to use based on tsoil
  if (tsoil >= params.coldSoilThreshold) {  // use normal (warm temp.) params
    tempEffect = params.baseSoilResp * pow(params.soilRespQ10, tsoil / 10);
  } else {  // use cold temp. params
    tempEffect =
        params.baseSoilRespCold * pow(params.soilRespQ10Cold, tsoil / 10);
  }
#else  // SEASONAL_R_SOIL FALSE -> always use normal params

  tempEffect = params.baseSoilResp * pow(params.soilRespQ10, tsoil / 10);

#endif

  fluxes.maintRespiration = envi.soil * moistEffect * tempEffect;
#endif

#endif
}

// Unused - remove?
// double microbeQualityEfficiency(double soilQuality) {
//
//   return params.efficiency;  // Efficiency an increasing function of quality
// }

void microbeGrowth(void) {
#if SOIL_MULTIPOOL
  int counter;  // Counter of quality pools
  for (counter = 0; counter < NUMBER_SOIL_CARBON_POOLS; counter++) {
#if SOIL_QUALITY

    double soilQuality;
    double ingestionCoeff;

    // Ensure that the quality will never be zero
    soilQuality = (counter + 1) / NUMBER_SOIL_CARBON_POOLS;

    ingestionCoeff = params.maxIngestionRate;

    // Scale this proportional to total soil
    fluxes.microbeIngestion[counter] =
        ingestionCoeff * envi.soil[counter] / trackers.totSoilC;
#endif  // We need code in here if we just have a rate coefficient pool
  }
#elif MICROBES
  double baseRate = params.maxIngestionRate * envi.soil /
                    (params.halfSatIngestion + envi.soil);

  // Flux that microbes remove from soil  (mg C g soil day)
  fluxes.microbeIngestion = baseRate * envi.microbeC;
#endif
}

// Now we need to calculate the production of the soil carbon pool, but we are
// going to have some outputs here that become inputs.  We need to output total
// respiration of this pool, as this gets fed back into the optimization

// soil quality is always the current counter divided by the number of total
// pools. Here we update the soil carbon pools and report the total respiration
// rate across all pools (it is easier this way than updating the carbon pools
// separately because then we don't have to store all the fluxes in a separate
// vector)

// find what soil carbon pool the litter (wood or leaf) enters into
int litterInputPool(double qualityComponent) {
  // Ensure that the input pool will never be 0
  return (int)floor(qualityComponent * NUMBER_SOIL_CARBON_POOLS);
}

void soilDegradation(void) {

  double soilWater;
#if MODEL_WATER  // take soilWater from environment
  soilWater = envi.soilWater;
#else  // take  soilWater from climate drivers
  soilWater = climate->soilWetness * params.soilWHC;
#endif

  calcMaintenanceRespiration(climate->tsoil, soilWater, params.soilWHC);

#if SOIL_MULTIPOOL
  int counter;  // Counter of different soil pools
  int woodLitterInput, leafLitterInput;  // The particular pool litter enters
                                         // into
  double litterInput;  // The litter rate into a pool

  double totResp, poolResp;  // Respiration rate summed across all pools

  microbeGrowth();
#if SOIL_QUALITY

  double microbeEff;

  double soilQuality;

  totResp = 0;

  // fluxes.woodLitter gives us the amount in the pool; we adjust these by 1
  // because the input pool will never be 0
  woodLitterInput = litterInputPool(params.qualityWood);

  // fluxes.leafLitter gives us the amount in the pool
  leafLitterInput = litterInputPool(params.qualityLeaf);

  for (counter = NUMBER_SOIL_CARBON_POOLS - 1; -1 < counter; counter--) {
    litterInput = 0;  // Initialize the total litter input with every loop

    soilQuality = (counter + 1) / NUMBER_SOIL_CARBON_POOLS;

    // calculate the microbial efficiency
    microbeEff = params.efficiency * soilQuality;

    poolResp = (1 - microbeEff) * fluxes.microbeIngestion[counter];
    // Add in growth + maintenance respiration
    totResp += poolResp + fluxes.maintRespiration[counter];

    if (woodLitterInput == counter) {
      litterInput += fluxes.woodLitter * climate->length;
    }

    if (leafLitterInput == counter) {
      litterInput += fluxes.leafLitter * climate->length;
    }

#if (counter == 0)
    envi.soil[counter] += (litterInput - fluxes.microbeIngestion[counter] -
                           fluxes.maintRespiration[counter]) *
                          climate->length;  // Transfer from this pool
#else
    envi.soil[counter] += (litterInput - fluxes.microbeIngestion[counter] -
                           fluxes.maintRespiration[counter]) *
                          climate->length;  // Transfer from this pool
    envi.soil[counter - 1] += (microbeEff * fluxes.microbeIngestion[counter]) *
                              climate->length;  // Transfer into next pool

#endif
  }
  // Do the roots.  If we don't model roots, the value of these fluxes will be
  // zero.

  envi.soil[NUMBER_SOIL_CARBON_POOLS - 1] +=
      (fluxes.coarseRootLoss + fluxes.fineRootLoss) * climate->length;
  fluxes.rSoil = totResp;
  // #else    This is the loop for no quality model

#endif
#elif MICROBES
  microbeGrowth();
  double microbeEff;

#if STOICHIOMETRY
  double microbeAdjustment;
  microbeAdjustment = (params.totNitrogen - params.microbeNC * envi.microbeC) /
                      envi.soil / params.microbeNC;
#if microbeAdjustment > 1
  microbeEff = params.efficiency;
#else
  microbeEff = params.efficiency * microbeAdjustment;
#endif
#else  // Now we do the single pool model

  microbeEff = params.efficiency;
#endif

  envi.soil +=
      (fluxes.coarseRootLoss + fluxes.fineRootLoss + fluxes.woodLitter +
       fluxes.leafLitter - fluxes.microbeIngestion) *
      climate->length;
  envi.microbeC += (microbeEff * fluxes.microbeIngestion + fluxes.soilPulse -
                    fluxes.maintRespiration) *
                   climate->length;

  fluxes.rSoil =
      fluxes.maintRespiration + (1 - microbeEff) * fluxes.microbeIngestion;

#elif LITTER_POOL  // If LITTER_POOL = 1, then all other bets are off
  envi.litter += (fluxes.woodLitter + fluxes.leafLitter - fluxes.litterToSoil -
                  fluxes.rLitter) *
                 climate->length;

  envi.soil += (fluxes.coarseRootLoss + fluxes.fineRootLoss +
                fluxes.litterToSoil - fluxes.rSoil) *
               climate->length;

#else  // Normal pool (single pool, no microbes)
  fluxes.rSoil = fluxes.maintRespiration;
  envi.soil += (fluxes.coarseRootLoss + fluxes.fineRootLoss +
                fluxes.woodLitter + fluxes.leafLitter - fluxes.rSoil) *
               climate->length;
#endif

  // Update roots.  If we don't model roots, these fluxes will be zero.
  envi.coarseRootC +=
      (fluxes.coarseRootCreation - fluxes.coarseRootLoss - fluxes.rCoarseRoot) *
      climate->length;
  envi.fineRootC +=
      (fluxes.fineRootCreation - fluxes.fineRootLoss - fluxes.rFineRoot) *
      climate->length;
}

////

// !!! general, multi-purpose functions !!!

// !!! functions to calculate fluxes !!!
// all fluxes are on a per-day basis

// transfer of carbon from plant woody material to litter, in g C * m^-2 ground
// area * day^-1 (includes above-ground and roots)
double woodLitterF(double plantWoodC) {
  return plantWoodC * params.woodTurnoverRate;  // turnover rate is fraction
                                                // lost per day
}

void calculateFluxes(void) {
  // auxiliary variables:
  double baseFolResp;
  double potGrossPsn;  // potential photosynthesis, without water stress
  double dWater;
  double lai;  // m^2 leaf/m^2 ground (calculated from plantLeafC)
  // double litterBreakdown; /* total litter breakdown (i.e. litterToSoil +
  // rLitter) (g C/m^2 ground/day) */
  double folResp, woodResp;  // maintenance respiration terms, g C * m^-2 ground
                             // area * day^-1
  double litterWater, soilWater; /* amount of water in litter and soil (cm)
          taken from either environment or climate drivers, depending on value
          of MODEL_WATER */

#if GROWTH_RESP
  double growthResp;  // g C * m^-2 ground area * day^-1
#endif

#if COMPLEX_WATER
  double netRain;  // rain - immedEvap (cm/day)
#endif

#if MODEL_WATER  // take litterWater and soilWater from environment
  litterWater = envi.litterWater;
  soilWater = envi.soilWater;
#else  // take litterWater and soilWater from climate drivers
  litterWater = climate->soilWetness * params.litterWHC; /* assume wetness is
              uniform throughout all layers (probably unrealistic, but it
              shouldn't matter too much) */
  soilWater = climate->soilWetness * params.soilWHC;
#endif

  lai = envi.plantLeafC / params.leafCSpWt;  // current lai

  potPsn(&potGrossPsn, &baseFolResp, lai, climate->tair, climate->vpd,
         climate->par, climate->day);
  moisture(&(fluxes.transpiration), &dWater, potGrossPsn, climate->vpd,
           soilWater);

#if MODEL_WATER  // water modeling happens here:

#if COMPLEX_WATER
  calcPrecip(&(fluxes.rain), &(fluxes.snowFall), &(fluxes.immedEvap), lai);
  netRain = fluxes.rain - fluxes.immedEvap;
  snowPack(&(fluxes.snowMelt), &(fluxes.sublimation), fluxes.snowFall);
  soilWaterFluxes(&(fluxes.fastFlow), &(fluxes.evaporation),
                  &(fluxes.topDrainage), &(fluxes.bottomDrainage), netRain,
                  fluxes.snowMelt, fluxes.transpiration, litterWater,
                  soilWater);
#else
  simpleWaterFlow(&(fluxes.rain), &(fluxes.snowFall), &(fluxes.immedEvap),
                  &(fluxes.snowMelt), &(fluxes.sublimation), &(fluxes.fastFlow),
                  &(fluxes.evaporation), &(fluxes.topDrainage),
                  &(fluxes.bottomDrainage), soilWater, envi.snow,
                  climate->precip, climate->tair, climate->length,
                  fluxes.transpiration);
#endif  // COMPLEX_WATER

#else  // MODEL_WATER = 0: set all water fluxes to 0
  fluxes.rain = fluxes.snowFall = fluxes.immedEvap = fluxes.snowMelt =
      fluxes.sublimation = fluxes.fastFlow = fluxes.evaporation =
          fluxes.topDrainage = fluxes.bottomDrainage = 0;
#endif  // MODEL_WATER

  getGpp(&(fluxes.photosynthesis), potGrossPsn, dWater);

#if GROWTH_RESP
  vegResp2(&folResp, &woodResp, &growthResp, baseFolResp,
           fluxes.photosynthesis);
  fluxes.rVeg = folResp + woodResp + growthResp;
  fluxes.rWood = woodResp;
  fluxes.rLeaf = folResp + growthResp;
#else
  vegResp(&folResp, &woodResp, baseFolResp);
  fluxes.rVeg = folResp + woodResp;
  fluxes.rWood = woodResp;
  fluxes.rLeaf = folResp;
#endif

  leafFluxes(&(fluxes.leafCreation), &(fluxes.leafLitter), envi.plantLeafC);

#if LITTER_POOL
  litterBreakdown =
      soilBreakdown(envi.litter, params.litterBreakdownRate, litterWater,
                    params.litterWHC, climate->tsoil, params.soilRespQ10);
  fluxes.rLitter = litterBreakdown * params.fracLitterRespired;
  fluxes.litterToSoil = litterBreakdown * (1.0 - params.fracLitterRespired);
  // NOTE: right now, we don't have capability to use separate cold soil params
  // for litter
#else
  //  litterBreakdown = 0;
  fluxes.rLitter = 0;
  fluxes.litterToSoil = 0;
#endif

  // finally, calculate fluxes that we haven't already calculated:

  fluxes.woodLitter = woodLitterF(envi.plantWoodC);

#if ROOTS
  double coarseExudate, fineExudate;  // exudates in and out of soil
  double npp, gppSoil;  // running means of our tracker variables

  npp = getMeanTrackerMean(meanNPP);
  gppSoil = getMeanTrackerMean(meanGPP);
  if (npp > 0) {
    fluxes.coarseRootCreation =
        (1 - params.leafAllocation - params.fineRootAllocation -
         params.woodAllocation) *
        npp;
    fluxes.fineRootCreation = params.fineRootAllocation * npp;
    fluxes.woodCreation = params.woodAllocation * npp;
  } else {
    fluxes.coarseRootCreation = 0;
    fluxes.fineRootCreation = 0;
    fluxes.woodCreation = 0;
  }

  if ((gppSoil > 0) & (envi.fineRootC > 0)) {
    coarseExudate = params.coarseRootExudation * gppSoil;
    fineExudate = params.fineRootExudation * gppSoil;
  } else {
    fineExudate = 0;
    coarseExudate = 0;
  }

  fluxes.coarseRootLoss = (1 - params.microbePulseEff) * coarseExudate +
                          params.coarseRootTurnoverRate * envi.coarseRootC;
  fluxes.fineRootLoss = (1 - params.microbePulseEff) * fineExudate +
                        params.fineRootTurnoverRate * envi.fineRootC;

  // fluxes that get added to microbe pool
  fluxes.soilPulse = params.microbePulseEff * (coarseExudate + fineExudate);

  calcRootResp(&fluxes.rCoarseRoot, params.coarseRootQ10,
               params.baseCoarseRootResp, envi.coarseRootC);
  calcRootResp(&fluxes.rFineRoot, params.fineRootQ10, params.baseFineRootResp,
               envi.fineRootC);

#else  // If we don't model roots, then all these fluxes will be zero

  fluxes.rCoarseRoot = 0;
  fluxes.rFineRoot = 0;
  fluxes.coarseRootCreation = 0;
  fluxes.fineRootCreation = 0;
  fluxes.woodCreation = 0;
  fluxes.coarseRootLoss = 0;
  fluxes.fineRootLoss = 0;
  fluxes.soilPulse = 0;

#endif

  // printf("%f %f %f\n", fluxes.rLitter*climate->length,
  // fluxes.rSoil*climate->length,
  // (fluxes.leafLitter+fluxes.woodLitter)*climate->length);

  // printf("%f %f %f %f %f\n", fluxes.photosynthesis*climate->length,
  // folResp*climate->length, woodResp*climate->length,
  // fluxes.rSoil*climate->length, fluxes.rLitter*climate->length);

  // printf("%f %f %f %f %f\n", fluxes.leafLitter*climate->length,
  // fluxes.woodLitter*climate->length, fluxes.rVeg*climate->length,
  // fluxes.rSoil*climate->length, fluxes.photosynthesis*climate->length);

  /* diagnosis: print water fluxes:
  printf("%f %f %f %f %f %f %f %f %f %f\n",
   fluxes.rain*climate->length, fluxes.snowFall*climate->length,
  fluxes.immedEvap*climate->length, fluxes.snowMelt*climate->length,
  fluxes.sublimation*climate->length, fluxes.fastFlow*climate->length,
  fluxes.evaporation*climate->length, fluxes.topDrainage*climate->length,
   fluxes.bottomDrainage*climate->length, fluxes.transpiration*climate->length);
  */

  /* printf("%f %f %f\n", fluxes.photosynthesis*climate->length,
   fluxes.transpiration*climate->length, (fluxes.transpiration > 0) ?
   (fluxes.photosynthesis/fluxes.transpiration) : 0);
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
  trackers.soilWetnessFrac = envi.soilWater / params.soilWHC;
  trackers.fa = 0.0;
  trackers.fr = 0.0;
  trackers.totSoilC = 0.0;
  trackers.rSoil = 0.0;

  trackers.rRoot = 0.0;

  trackers.totSoilC = params.soilInit;
  trackers.rAboveground = 0.0;
  trackers.fpar = 0.0;

  trackers.plantWoodC = 0.0;
  trackers.LAI = 0.0;
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

#if LITTER_POOL
  ensureNonNegative(&(envi.litter), 0);
#endif

#if SOIL_MULTIPOOL
  int counter;
  for (counter = 0; counter < NUMBER_SOIL_CARBON_POOLS; counter++) {
    ensureNonNegative(&(envi.soil[counter]), 0);
  }
#else
  ensureNonNegative(&(envi.soil), 0);
#endif

  ensureNonNegative(&(envi.coarseRootC), 0);
  ensureNonNegative(&(envi.fineRootC), 0);
  ensureNonNegative(&(envi.microbeC), 0);

#if MODEL_WATER

#if LITTER_WATER
  ensureNonNegative(&(envi.litterWater), 0);
#endif

  ensureNonNegative(&(envi.soilWater), 0);
  /* In the case of snow, the model has very different behavior for a snow pack
     of 0 vs. a snow pack of slightly greater than 0 (e.g. no soil evaporation
     if snow > 0). Thus to avoid large errors due to small rounding errors,
     we'll set snow = 0 any time it falls below TINY, the assumption being that
     if snow < TINY, then it was really supposed to be 0, but isn't because of
     rounding errors.*/
  ensureNonNegative(&(envi.snow), TINY);
#endif
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

  trackers.fa = trackers.gpp - (fluxes.rLeaf) * climate->length;
  trackers.fr = trackers.rh + (fluxes.rWood) * climate->length;

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

  trackers.evapotranspiration = (fluxes.transpiration + fluxes.immedEvap +
                                 fluxes.evaporation + fluxes.sublimation) *
                                climate->length;

  trackers.soilWetnessFrac =
      (oldSoilWater + envi.soilWater) / (2.0 * params.soilWHC);
  trackers.totSoilC = 0;  // Set this to 0, and then we add to it
#if SOIL_MULTIPOOL
  int counter;

  for (counter = 0; counter < NUMBER_SOIL_CARBON_POOLS; counter++) {
    trackers.totSoilC += envi.soil[counter];
  }
#else
  trackers.totSoilC += envi.soil;
#endif

  trackers.fpar = getMeanTrackerMean(meanFPAR);

  trackers.LAI = envi.plantLeafC / params.leafCSpWt;
  trackers.yearlyLitter += fluxes.leafLitter;
  trackers.plantWoodC = envi.plantWoodC;
  // note this variable is added for Howland forest multi-model comparison
  // includes ONLY leaf litter

  // mean of soil wetness at start of time step at soil wetness at end of time
  // step - assume linear
}

/*!
 * \brief Process events for current location/year/day
 *
 * For a given year and day (as determined by the global `climate`
 * pointer), process all events listed in the global `events` pointer for the
 * referenced location.
 *
 * For each event, modify state variables according to the model for that event,
 * and write a row to events.out listing the modified variables and the delta
 * applied.
 */
#if EVENT_HANDLER
void processEvents(void) {
  // This should be in events.h/c, but with all the global state defined in this
  // file, let's leave it here for now. Maybe someday we will factor that out.

  // If event starts off NULL, this function will just fall through, as it
  // should.
  const int climYear = climate->year;
  const int climDay = climate->day;

  // The events file has been tested on read, so we know this event list should
  // be in chrono order. However, we need to check to make sure the current
  // event is not in the past, as that would indicate an event that did not have
  // a corresponding climate file record.
  while (event != NULL && event->year <= climYear && event->day <= climDay) {
    if (event->year < climYear || event->day < climDay) {
      printf("Agronomic event found for year: %d day: %d that does not "
             "have a corresponding record in the climate file\n",
             event->year, event->day);
      exit(EXIT_CODE_INPUT_FILE_ERROR);
    }
    switch (event->type) {
      // Implementation TBD, as we enable the various event types
      case IRRIGATION: {
        const IrrigationParams *irrParams = event->eventParams;
        const double amount = irrParams->amountAdded;
        double soilAmount, evapAmount;
        if (irrParams->method == CANOPY) {
          // Part of the irrigation evaporates, and the rest makes it to the
          // soil. Evaporated fraction:
          evapAmount = params.immedEvapFrac * amount;
          // Soil fraction:
          soilAmount = amount - evapAmount;
        } else if (irrParams->method == SOIL) {
          // All goes to the soil
          evapAmount = 0.0;
          soilAmount = amount;
        } else {
          printf("Unknown irrigation method type: %d\n", irrParams->method);
          exit(EXIT_CODE_UNKNOWN_EVENT_TYPE_OR_PARAM);
        }
        fluxes.immedEvap += evapAmount;
        envi.soilWater += soilAmount;
        writeEventOut(eventOutFile, event, 2, "envi.soilWater", soilAmount,
                      "fluxes.immedEvap", evapAmount);
      } break;
      case PLANTING: {
        const PlantingParams *plantParams = event->eventParams;
        const double leafC = plantParams->leafC;
        const double woodC = plantParams->woodC;
        const double fineRootC = plantParams->fineRootC;
        const double coarseRootC = plantParams->coarseRootC;

        // Update the pools
        envi.plantLeafC += leafC;
        envi.plantWoodC += woodC;
        envi.fineRootC += fineRootC;
        envi.coarseRootC += coarseRootC;

        // FUTURE: allocate to N pools

        writeEventOut(eventOutFile, event, 4, "envi.plantLeafC", leafC,
                      "envi.plantWoodC", woodC, "envi.fineRootC", fineRootC,
                      "envi.coarseRootC", coarseRootC);
      } break;
      case HARVEST: {
        // Harvest can both remove biomass and move biomass to the litter pool
        const HarvestParams *harvParams = event->eventParams;
        const double fracRA = harvParams->fractionRemovedAbove;
        const double fracTA = harvParams->fractionTransferredAbove;
        const double fracRB = harvParams->fractionRemovedBelow;
        const double fracTB = harvParams->fractionTransferredBelow;

        // Litter increase
        const double litterAdd = fracTA * (envi.plantLeafC + envi.plantWoodC) +
                                 fracTB * (envi.fineRootC + envi.coarseRootC);
        // Pool reductions, counting both mass moved to litter and removed by
        // the harvest itself. Above-ground changes:
        const double leafDelta = -envi.plantLeafC * (fracRA + fracTA);
        const double woodDelta = -envi.plantWoodC * (fracRA + fracTA);
        // Below-ground changes:
        const double fineDelta = -envi.fineRootC * (fracRB + fracTB);
        const double coarseDelta = -envi.coarseRootC * (fracRB + fracTB);

        // Pool updates:
        envi.litter += litterAdd;
        envi.plantLeafC += leafDelta;
        envi.plantWoodC += woodDelta;
        envi.fineRootC += fineDelta;
        envi.coarseRootC += coarseDelta;

        // FUTURE: move/remove biomass in N pools

        writeEventOut(eventOutFile, event, 5, "env.litter", litterAdd,
                      "envi.plantLeafC", leafDelta, "envi.plantWoodC",
                      woodDelta, "envi.fineRootC", fineDelta,
                      "envi.coarseRootC", coarseDelta);
      } break;
      case TILLAGE:
        // TBD
        printf("Tillage events not yet implemented\n");
        break;
      case FERTILIZATION:
        // TBD
        printf("Fertilization events not yet implemented\n");
        break;
      default:
        printf("Unknown event type (%d) in processEvents()\n", event->type);
        exit(EXIT_CODE_UNKNOWN_EVENT_TYPE_OR_PARAM);
    }

    event = event->nextEvent;
  }
}
#endif

// !!! main runner function !!!

// calculate all fluxes and update state for this time step
// we calculate all fluxes before updating state in case flux calculations
// depend on the old state
void updateState(void) {
  double npp;  // net primary productivity, g C * m^-2 ground area * day^-1
  double oldSoilWater;  // how much soil water was there before we updated it?
                        // Used in trackers
  int err;
  oldSoilWater = envi.soilWater;

  calculateFluxes();

  // update the stocks, with fluxes adjusted for length of time step:
  envi.plantWoodC += (fluxes.photosynthesis + fluxes.woodCreation -
                      fluxes.leafCreation - fluxes.woodLitter - fluxes.rVeg -
                      fluxes.coarseRootCreation - fluxes.fineRootCreation) *
                     climate->length;
  envi.plantLeafC +=
      (fluxes.leafCreation - fluxes.leafLitter) * climate->length;

  soilDegradation();  // This updates all the soil functions

#if MODEL_WATER  // water pool updating happens here:
#if LITTER_WATER  // (2 soil water layers; litter water will only be on if
                  // complex water is also on)
  envi.litterWater +=
      (fluxes.rain + fluxes.snowMelt - fluxes.immedEvap - fluxes.fastFlow -
       fluxes.evaporation - fluxes.topDrainage) *
      climate->length;
  envi.soilWater +=
      (fluxes.topDrainage - fluxes.transpiration - fluxes.bottomDrainage) *
      climate->length;
#else  // LITTER_WATER = 0 (only one soil water layer)
       // note: some of these fluxes will always be 0 if complex
       // water is off
  envi.soilWater +=
      (fluxes.rain + fluxes.snowMelt - fluxes.immedEvap - fluxes.fastFlow -
       fluxes.evaporation - fluxes.transpiration - fluxes.bottomDrainage) *
      climate->length;
#endif  // LITTER_WATER

  // if COMPLEX_WATER = 0 or SNOW = 0, some or all of these fluxes will always
  // be 0
  envi.snow += (fluxes.snowFall - fluxes.snowMelt - fluxes.sublimation) *
               climate->length;

#endif  // MODEL_WATER

  ensureNonNegativeStocks();

#if EVENT_HANDLER
  // Process events for this location/year/day, AFTER updates are made to fluxes
  // and state variables above. Events are (currently, Jan 25, 2025) handled as
  // instantaneous deltas to relevant state (envi and fluxes fields),
  processEvents();
#endif

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

  updateTrackers(oldSoilWater);
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

// Setup model to run at given location (0-indexing: if only one location, loc
// should be 0)
void setupModel(void) {

  // a test: use constant (measured) soil respiration:
  // make it so soil resp. is 5.2 g C m-2 day-1 at 10 degrees C,
  // moisture-saturated soil, and soil C = init. soil C params.baseSoilResp =
  // ((5.2 * 365.0)/params.soilInit)/params.soilRespQ10;

  // ensure that all the allocation parameters sum up to something less than
  // one:
#if ROOTS
  ensureAllocation();
#endif

// If we aren't explicitly modeling microbe pool, then do not have a pulse to
// microbes, exudates go directly to the soil
#if !MICROBES
  params.microbePulseEff = 0;
#endif

  // change units of parameters:
  params.baseVegResp /= 365.0;  // change from per-year to per-day rate
  params.litterBreakdownRate /= 365.0;
  params.baseSoilResp /= 365.0;
  params.baseSoilRespCold /= 365.0;
  params.woodTurnoverRate /= 365.0;
  params.leafTurnoverRate /= 365.0;

  // calculate additional parameters:
  params.psnTMax =
      params.psnTOpt + (params.psnTOpt - params.psnTMin);  // assumed
                                                           // symmetrical

#if ROOTS
  envi.plantWoodC =
      (1 - params.coarseRootFrac - params.fineRootFrac) * params.plantWoodInit;
#else
  envi.plantWoodC = params.plantWoodInit;
#endif

  envi.plantLeafC = params.laiInit * params.leafCSpWt;
  envi.litter = params.litterInit;

  // If SOIL_QUALITY, split initial soilCarbon equally among all the pools
#if SOIL_MULTIPOOL
  int counter;

  for (counter = 0; counter < NUMBER_SOIL_CARBON_POOLS; counter++) {

    envi.soil[counter] = params.soilInit / NUMBER_SOIL_CARBON_POOLS;
  }
  trackers.totSoilC = params.soilInit;
#else
  envi.soil = params.soilInit;

#endif

#if SOIL_QUALITY
  params.maxIngestionRate =
      params.maxIngestionRate * 24 * params.microbeInit / 1000;
  // change from per hour to per day rate, and then multiply by microbial
  // concentration (mg C / g soil).

#else
  // change from per hour to per day rate
  params.maxIngestionRate = params.maxIngestionRate * 24;
#endif

  envi.microbeC = params.microbeInit * params.soilInit / 1000;  // convert to gC
                                                                // m-2

  params.totNitrogen = params.totNitrogen * params.soilInit;  // convert to gC
                                                              // m-2

  params.fineRootTurnoverRate /= 365.0;
  params.coarseRootTurnoverRate /= 365.0;

  params.baseCoarseRootResp /= 365.0;
  params.baseFineRootResp /= 365.0;
  params.baseMicrobeResp = params.baseMicrobeResp * 24;  // change from per hour
                                                         // to per day rate

  envi.coarseRootC = params.coarseRootFrac * params.plantWoodInit;
  envi.fineRootC = params.fineRootFrac * params.plantWoodInit;

  envi.litterWater = params.litterWFracInit * params.litterWHC;
  if (envi.litterWater < 0) {
    envi.litterWater = 0;
  } else if (envi.litterWater > params.litterWHC) {
    envi.litterWater = params.litterWHC;
  }

  envi.soilWater = params.soilWFracInit * params.soilWHC;
  if (envi.soilWater < 0) {
    envi.soilWater = 0;
  } else if (envi.soilWater > params.soilWHC) {
    envi.soilWater = params.soilWHC;
  }

  envi.snow = params.snowInit;

  climate = firstClimate;

  initTrackers();
  initPhenologyTrackers();
  resetMeanTracker(meanNPP, 0);  // initialize with mean NPP (over last
                                 // MEAN_NPP_DAYS) of 0
  resetMeanTracker(meanGPP, 0);  // initialize with mean NPP (over last
                                 // MEAN_GPP_DAYS) of 0
  resetMeanTracker(meanFPAR, 0);  // initialize with mean FPAR (over last
                                  // MEAN_FPAR_DAYS) of 0
}

// Setup events at given location
#if EVENT_HANDLER
void setupEvents() { event = events; }
#endif

// See sipnet.h
void runModelOutput(FILE *out, OutputItems *outputItems, int printHeader) {
  if ((out != NULL) && printHeader) {
    outputHeader(out);
  }

  setupModel();
#if EVENT_HANDLER
  setupEvents();
#endif

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

// write to file which model components are turned on
// (i.e. the value of the #DEFINE's at the top of file)
// pre: out is open for writing
void printModelComponents(FILE *out) {
  fprintf(out, "Optional model components (0 = off, 1 = on):\n");

  fprintf(out, "ALTERNATIVE_TRANS = %d\n", ALTERNATIVE_TRANS);
  fprintf(out, "BALL_BERRY = %d\n", BALL_BERRY);
  fprintf(out, "PENMAN_MONTEITH_TRANS = %d\n", PENMAN_MONTEITH_TRANS);

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
  fprintf(out, "LITTER_POOL = %d\n", LITTER_POOL);
  fprintf(out, "SOIL_MULTIPOOL = %d\n", SOIL_MULTIPOOL);
  fprintf(out, "NUMBER_SOIL_CARBON_POOLS = %d\n", NUMBER_SOIL_CARBON_POOLS);
  fprintf(out, "SOIL_QUALITY = %d\n", SOIL_QUALITY);
  fprintf(out, "MICROBES = %d\n", MICROBES);
  fprintf(out, "STOICHIOMETRY = %d\n", STOICHIOMETRY);
  fprintf(out, "ROOTS = %d\n", ROOTS);

  fprintf(out, "\n");

  fprintf(out, "Tracker variables settings:\n");
  fprintf(out, "MEAN_NPP_DAYS = %d\n", MEAN_NPP_DAYS);
  fprintf(out, "MEAN_GPP_SOIL_DAYS = %d\n", MEAN_GPP_SOIL_DAYS);
  fprintf(out, "MEAN_FPAR_DAYS = %d\n", MEAN_FPAR_DAYS);

  fprintf(out, "\n");
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
  meanFPAR = newMeanTracker(0, MEAN_FPAR_DAYS, MEAN_FPAR_MAX_ENTRIES);
}

#if EVENT_HANDLER
// See sipnet.h
void initEvents(char *eventFile, int printHeader) {
  events = readEventData(eventFile);
  eventOutFile = openEventOutFile(printHeader);
}
#endif

// See sipnet.h
void cleanupModel() {
  freeClimateList();
  deallocateMeanTracker(meanNPP);
  deallocateMeanTracker(meanGPP);
  deallocateMeanTracker(meanFPAR);
#if EVENT_HANDLER
  freeEventList();
  closeEventOutFile(eventOutFile);
#endif
}

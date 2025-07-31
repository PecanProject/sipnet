#ifndef SIPNET_STATE_H
#define SIPNET_STATE_H

// See sipnet.c for the list of references cited below. In short:
// [1] Braswell et al., 2005
// [2] Sacks et al., 2006
// [3] Chapter titled "PROCESS-BASED MODELING OF SOIL RESPIRATION FLUXES USING
//     A MODEL-DATA FUSION ANALYSIS", author unknown

typedef struct ClimateVars ClimateNode;

struct ClimateVars {
  // year of start of this timestep
  int year;
  // day of start of this timestep (1 = Jan 1.)
  int day;
  // time of start of this timestep (hour.fraction - e.g. noon= 12.0,
  // midnight = 0.0)
  double time;
  // length of this timestep (in days) - allow variable-length timesteps
  double length;
  // avg. air temp for this time step (degrees C)
  double tair;
  // avg. soil temp for this time step (degrees C)
  double tsoil;
  // average par for this time step (Einsteins * m^-2 ground area * day^-1)
  // NOTE: input is in Einsteins * m^-2 ground area, summed over entire time
  // step
  double par;
  // total precip. for this time step (cm water equiv. - either rain or snow)
  // NOTE: input is in mm
  double precip;
  // average vapor pressure deficit (kPa).  NOTE: input is in Pa
  double vpd;
  //  average vapor pressure deficit between soil and air (kPa). NOTE: input is
  //  in Pa.
  //  Differs from vpd in that saturation vapor pressure calculated using Tsoil
  //  rather than Tair
  double vpdSoil;
  //  average vapor pressure in canopy airspace (kPa). NOTE: input is in Pa
  double vPress;
  // avg. wind speed (m/s)
  double wspd;
  // growing degree days from Jan. 1 to now. NOTE: Calculated, *not* read from
  // file
  double gdd;

  ClimateNode *nextClim;
};

#define NUM_CLIM_FILE_COLS 12
#define NUM_CLIM_FILE_COLS_LEGACY (NUM_CLIM_FILE_COLS + 2)

// Model parameters which can change from one run to the next. These include
// initializations of state.
// Initial values are read in from a file, or calculated at start of model.
// If any parameters are added here, and these parameters are to be read from
// file, be sure to add them to the readParamData function, below

// Parameter values are read in from <inputFile>.param
typedef struct Parameters {
  // *****
  // Params from [1] Braswell et al. (2005)

  //
  // Initial state values

  // g C * m^-2 ground area in wood (above-ground + roots)
  // :: C_(W,0) in [1]
  double plantWoodInit;
  // initial leaf area, m^2 leaves * m^-2 ground area (multiply by leafCSpWt to
  // get initial plant leaf C, which is C_(L,0) in [1])
  // :: used to derive C_L_0 in [1]
  double laiInit;
  // initial soil C content, g C * m^-2 ground area
  // :: C_(S,0) in [1]
  double soilInit;
  // unitless: fraction of soilWHC
  // :: used to derive W_0 in [1]
  double soilWFracInit;

  //
  // Photosynthesis

  // max photosynthesis assuming max. possible par, all intercepted, no temp,
  // water or vpd stress (nmol CO2 * g^-1 leaf * sec^-1)
  // :: A_max in [1]
  double aMax;
  // avg. daily aMax as fraction of instantaneous
  // :: A_d in [1]
  double aMaxFrac;
  // basal foliage resp. rate, as % of aMax
  // :: K_f in [1]
  double baseFolRespFrac;
  // min and optimal temps at which net photosynthesis occurs (degrees C)
  // :: T_min and T_opt in [1]
  double psnTMin, psnTOpt;
  // slope of VPD-psn relationship
  // :: K_VPD in [1]
  double dVpdSlope;
  // exponent for D_VPD calculation via dVpd = 1 - dVpdSlope * vpd^dVpdExp
  // Note: this is explicitly shown as 2 in [1] - see (A10)
  // :: no direct analog in [1]
  double dVpdExp;
  // par at which photosynthesis occurs at 1/2 theoretical maximum
  // (Einsteins * m^-2 ground area * day^-1)
  // :: PAR_1/2 in [1]
  double halfSatPar;
  // light attenuation coefficient
  // :: k in [1]
  double attenuation;

  //
  // Phenology related

  // day when leaves appear
  // :: D_on in [1]
  double leafOnDay;
  // day when leaves disappear
  // :: D_off in [1]
  double leafOffDay;
  // No L_max from [1], superseded by...

  //
  // Autotrophic respiration

  // vegetation maintenance respiration at 0 degrees C
  // (g C respired * g^-1 plant C * day^-1)
  // Only counts plant wood C - leaves handled elsewhere (both above and
  // below-ground: assumed for now to have same resp. rate)
  // NOTE: read in as per-year rate!
  // :: K_A in [1]
  double baseVegResp;
  // scalar determining effect of temp on veg. resp.; Q_10_v in [1]
  double vegRespQ10;

  //
  // Soil respiration

  // soil respiration at 0 degrees C and max soil moisture
  // (g C respired * g^-1 soil C * day^-1)
  // NOTE: read in as per-year rate!
  // :: K_H in [1]
  double baseSoilResp;
  // scalar determining effect of temp on soil resp.; Q_10_s in [1]
  double soilRespQ10;

  //
  // Moisture related

  // fraction of plant available soil water which can be removed in one day
  // without water stress occurring
  // :: f in [1]
  double waterRemoveFrac;
  // water use efficiency constant
  // :: K_WUE in [1]
  double wueConst;
  // soil (transpiration layer) water holding capacity (cm)
  // :: W_c in [1]
  double soilWHC;
  // g C * m^-2 leaf area
  // :: SLW in [1]
  double leafCSpWt;
  // g leaf C * g^-1 leaf
  // :: C_frac in [1]
  double cFracLeaf;
  // average turnover rate of woody plant C, in fraction per day (leaf loss
  // handled separately). NOTE: read in as per-year rate!
  // :: K_w in [1]
  double woodTurnoverRate;

  // *****
  // Other params, provenance TBD
  //

  // initial state values:
  double litterInit;  // g C * m^-2 ground area
  double litterWFracInit;  // unitless: fraction of litterWHC
  double snowInit;  // cm water equiv.

  // phenology-related:
  double gddLeafOn;  // with gdd-based phenology, gdd threshold for leaf
                     // appearance
  double soilTempLeafOn;  // with soil temp-based phenology, soil temp threshold
                          // for leaf appearance
  double leafGrowth;  // additional leaf growth at start of growing season
                      // (g C * m^-2 ground)
  double fracLeafFall;  // additional fraction of leaves that fall at end of
                        // growing season
  double leafAllocation;  // fraction of NPP allocated to leaf growth
  double leafTurnoverRate;  // average turnover rate of leaves, in fraction per
                            // day. NOTE: read in as per-year rate!

  // autotrophic respiration:
  double growthRespFrac;  // growth resp. as fraction of (GPP - woodResp -
                          // folResp)
  double frozenSoilFolREff;  // amount that foliar resp. is shutdown if soil is
                             // frozen (0 = full shutdown, 1 = no shutdown)
  double frozenSoilThreshold;  // soil temperature below which frozenSoilFolREff
                               // and frozenSoilEff kick in (degrees C)

  // soil respiration:
  double litterBreakdownRate; /* rate at which litter is converted to
                 soil/respired at 0 degrees C and max soil moisture (g C broken
                 down * g^-1 litter C * day^-1) NOTE: read in as per-year rate
               */
  double fracLitterRespired; /* of the litter broken down, fraction respired
                (the rest is transferred to soil pool) */
  double soilRespMoistEffect;  // scalar determining effect of moisture on soil
                               // resp.

  // moisture-related:
  double frozenSoilEff;  // fraction of water that's available if soil is frozen
                         // (0 = none available, 1 = all still avail.)
                         // NOTE: if frozenSoilEff = 0, then shut down psn
  double litterWHC;  // litter (evaporative layer) water holding capacity (cm)
  double immedEvapFrac;  // fraction of rain that is immediately intercepted &
                         // evaporated
  double fastFlowFrac;  // fraction of water entering soil that goes directly to
                        // drainage
  double snowMelt;  // rate at which snow melts (cm water equiv./degree C/day)
  double rdConst;  // scalar determining amount of aerodynamic resistance
  double rSoilConst1, rSoilConst2;  // soil resistance =
                                    // e^(rSoilConst1 - rSoilConst2 * W1)
                                    // where W1 = (litterWater/litterWHC)
  double leafPoolDepth;  // leaf (evaporative) pool rim thickness in mm

  // calculated parameters:
  double psnTMax;  // degrees C - assumed symmetrical around psnTOpt

  // quality model parameters
  double efficiency;  // conversion efficiency of ingested carbon

  double maxIngestionRate;  // hr-1 - maximum ingestion rate of the microbe
  double halfSatIngestion;  // mg C g-1 soil - half saturation ingestion rate of
                            // microbe

  double microbeInit;  // mg C / g soil microbe initial carbon amount

  double fineRootFrac;  // fraction of wood carbon allocated to fine roots
  double coarseRootFrac;  // fraction of wood carbon that is coarse roots
  double fineRootAllocation;  // fraction of NPP allocated to fine roots
  double woodAllocation;  // fraction of NPP allocated to the roots
  double fineRootExudation;  // fraction of GPP exuded to the soil
  double coarseRootExudation;  // fraction of NPP exuded to the soil

  // Calculated param
  double coarseRootAllocation;  // fraction of NPP allocated to the coarse roots

  double fineRootTurnoverRate;  // turnover of fine roots (per year rate)
  double coarseRootTurnoverRate;  // turnover of coarse roots (per year rate)
  double baseFineRootResp;  // base respiration rate of fine roots  (per year
                            // rate)
  double baseCoarseRootResp;  // base respiration rate of coarse roots (per year
                              // rate)
  double fineRootQ10;  // Q10 of fine roots
  double coarseRootQ10;  // Q10 of coarse roots

  double baseMicrobeResp;  // base respiration rate of microbes
  double microbeQ10;  // Q10 of coarse roots
  double microbePulseEff;  // fraction of exudates that microbes immediately
                           // use.

  // OBSOLETE PARAMS
  // To be removed
  double baseSoilRespCold;  // OBSOLETE PARAM
                            // soil respiration at 0 degrees C and max soil
                            // moisture when tsoil < coldSoilThreshold
                            // (g C respired * g^-1 soil C day^-1)
                            // NOTE: read in as per-year rate!
  double soilRespQ10Cold;  // OBSOLETE PARAM
                           // scalar determining effect of temp on soil resp.
                           // when tsoil < coldSoilThreshold

  double coldSoilThreshold;  // OBSOLETE PARAM
                             // temp. at which use baseSoilRespCold and
                             // soilRespQ10Cold (if SEASONAL_R_SOIL true)
                             // (degrees C)
  double litWaterDrainRate;  // OBSOLETE PARAM
                             // rate at which litter water drains into lower
                             // layer when litter layer fully moisture-saturated
                             // (cm water/day)
  double E0;  // OBSOLETE PARAM  E0 in Lloyd-Taylor soil respiration function
  double T0;  // OBSOLETE PARAM  T0 in Lloyd-Taylor soil respiration function
  double m_ballBerry;  // OBSOLETE PARAM slope for the Ball Berry relationship
  double totNitrogen;  // OBSOLETE PARAM  Percentage nitrogen in soil
  double microbeNC;  // OBSOLETE PARAM  mg N / mg C - microbe N:C ratio
  double qualityLeaf;  // value for leaf litter quality
  double qualityWood;  // value for wood litter quality

} Params;

#define NUM_PARAMS (sizeof(Params) / sizeof(double))

// the state of the environment
typedef struct Environment {
  // From [1] Braswell et al. 2005
  double plantWoodC;  // carbon in plant wood (above-ground + roots) (g C * m^-2
                      // ground area)
  double plantLeafC;  // carbon in leaves (g C * m^-2 ground area)
  double soil;  // carbon in soil (g C * m^-2 ground area)
  double soilWater;  // plant available soil water (cm)

  // From other sources
  double litter;  // carbon in litter (g C * m^-2 ground area)
  double litterWater;  // water in litter (evaporative) layer (cm)
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

  double maintRespiration;  // Microbial maintenance respiration rate g C
                            // m-2 ground area day^-1; except when microbes are
                            // not in effect, and it is equivalent to rSoil, at
                            // least as describe in [1], eq (A20)
  double microbeIngestion;

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

  double rRoot;  // g C m-2 of root respiration
  double rSoil;  // Soil respiration (microbes+root)

  double rAboveground;  // Wood and foliar respiration
  double fpar;  // 8 day mean fractional photosynthetically active radiation
                // (percentage)
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

#endif  // SIPNET_STATE_H

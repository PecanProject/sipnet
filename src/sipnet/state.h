#ifndef SIPNET_STATE_H
#define SIPNET_STATE_H

// See sipnet.c for the list of references cited below. In short:
// [1] Braswell et al., 2005
// [2] Sacks et al., 2006
// [3] Zobitz et al, 2008
// [4] Zobitz (et al.?), date unknown, chapter 5 from unknown book
//
// Also of note, additions have been made to support the MAGIC
// (Modeling and Analysis of Greenhouse gases In Cropland) project
// for the California Air Resources Board

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

// Global vars
extern ClimateNode *firstClimate;  // pointer to first climate
extern ClimateNode *climate;  // current climate

#define NUM_CLIM_FILE_COLS 12
#define NUM_CLIM_FILE_COLS_LEGACY (NUM_CLIM_FILE_COLS + 2)

// Model parameters, which can change from one run to the next, include
// initializations of state. Initial values are read in from a file, or
// calculated at start of model.
//
// If any parameters are added here, and these parameters are to be read from
// file, be sure to add them to the readParamData function, below

// Parameter values are read in from <inputFile>.param
typedef struct Parameters {

  // ****************************************
  // Params from [1] Braswell et al. (2005)
  // ****************************************

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
  // calculated (not read) T max, assumed symmetrical around psnTOpt (degrees C)
  double psnTMax;
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
  // with gdd-based phenology, gdd threshold for leaf appearance
  // :: unlabeled in [1]
  double gddLeafOn;

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

  // ****************************************
  // Params from [2] Sacks et al. (2006)
  // ****************************************

  //
  // Initial state values

  // initial litter pool (g C * m^-2 ground area)
  // :: not directly listed in [2]
  double litterInit;
  // initial snowpack (cm water equivalent)
  // :: W_P,0 in [2]
  double snowInit;

  //
  // Moisture related

  // fraction of water that's available if soil is frozen
  // (0 = none available, 1 = all still avail.)
  // NOTE: if frozenSoilEff = 0, then shut down psn
  // :: not directly listed in [2]
  double frozenSoilEff;
  // fraction of rain that is immediately intercepted & evaporated
  // :: E in [2]
  double immedEvapFrac;
  // fraction of water entering soil that goes directly to drainage
  // :: F in [2]
  double fastFlowFrac;
  // rate at which snow melts (cm water equiv./degree C/day)
  // :: K_S in [2]
  double snowMelt;
  // scalar determining amount of aerodynamic resistance
  // :: R_d in [2]
  double rdConst;
  // parameters used to calculate soil resistance as
  // soil resistance = e^(rSoilConst1 - rSoilConst2 * W1)
  // where W1 = (water/WHC)
  // :: R_soil,1 and R_soil,2 in [2]
  double rSoilConst1, rSoilConst2;

  //
  // Phenology related

  // fraction of NPP allocated to leaf growth
  // :: NPP_L in [2]
  double leafAllocation;
  // average turnover rate of leaves, in fraction per day. NOTE: read in as
  // per-year rate!
  // :: K_L in [2]
  double leafTurnoverRate;

  //
  // Autotrophic respiration

  // amount that foliar resp. is shutdown if soil is frozen (0 = full shutdown,
  // 1 = no shutdown)
  // :: not directly listed in [2]
  double frozenSoilFolREff;
  // soil temperature below which frozenSoilFolREff and frozenSoilEff kick in
  // (degrees C)
  // :: T_s in [2]
  double frozenSoilThreshold;

  //
  // Soil respiration

  // rate at which litter is converted to soil/respired at 0 degrees C and max
  // soil moisture (g C broken down * g^-1 litter C * day^-1)
  // NOTE: read in as per-year rate
  // :: not directly listed in [2]
  double litterBreakdownRate;
  // of the litter broken down, fraction respired (the rest is transferred to
  // soil pool)
  // :: not directly listed in [2]
  double fracLitterRespired;

  // ****************************************
  // Params from [3] Zobitz et al. (2008)
  // ****************************************

  // Roots
  //

  // fraction of wood carbon allocated to fine roots
  double fineRootFrac;
  // fraction of wood carbon that is coarse roots
  double coarseRootFrac;
  // fraction of NPP allocated to the non-root wood
  double woodAllocation;
  // fraction of NPP allocated to fine roots
  double fineRootAllocation;
  // fraction of NPP allocated to the coarse roots (calculated param)
  double coarseRootAllocation;
  // fraction of GPP exuded to the soil
  double fineRootExudation;
  // fraction of GPP exuded to the soil
  double coarseRootExudation;
  // turnover of fine roots (per year rate)
  double fineRootTurnoverRate;
  // turnover of coarse roots (per year rate)
  double coarseRootTurnoverRate;
  // base respiration rate of fine roots  (per year rate)
  double baseFineRootResp;
  // base respiration rate of coarse roots (per year rate)
  double baseCoarseRootResp;
  // Q10 of fine roots
  double fineRootQ10;
  // Q10 of coarse roots
  double coarseRootQ10;

  // ****************************************
  // Params from [4] Zobitz et al. (draft)
  // ****************************************

  // mg C / g soil microbe initial carbon amount, as fraction of soil init C
  // :: equivalent to C_B,0 in [4]
  double microbeInit;
  // base respiration rate of microbes at 0 degrees C
  // :: K_B in [4]
  double baseMicrobeResp;
  // Q10 of microbes
  // :: Q10_B in [4]
  double microbeQ10;
  // fraction of exudates that microbes immediately use (microbe assimilation
  // efficiency of root exudates)
  // :: epsilon_R in [4]
  double microbePulseEff;
  // microbe efficiency to convert carbon to biomass
  // :: epsilon in [4]
  double efficiency;
  // microbial maximum ingestion rate (hr-1)
  // :: mu_MAX in [4]
  double maxIngestionRate;
  // half saturation ingestion rate of microbes (mg C g-1 soil)
  // Note: those units can't be correct, as we add this to envi.soil; must be
  //       (g C / m-2 ground area), as there are no conversions in the code.
  //       Or, this is a bug. (This param is listed as g C/m-2 in [4], so
  //       probably not a bug.)
  // :: theta_B in [4]
  double halfSatIngestion;

  // ****************************************
  // Other params, provenance TBD
  // ****************************************

  // phenology-related:
  // with soil temp-based phenology, soil temp threshold for leaf appearance
  double soilTempLeafOn;

  // The two params below seem to be used in leaf calculations as part of a
  // merging of approaches from [1] and [2]
  // additional leaf growth at start of growing season (g C * m^-2 ground)
  double leafGrowth;
  // additional fraction of leaves that fall at end of growing season
  double fracLeafFall;

  // autotrophic respiration:
  // growth resp. as fraction of (GPP - woodResp - folResp)
  // Note: that comment may not be correct, growthResp is calc'd as
  //       (mean GPP) * (growtheRespFrac)
  //       with no correction for woodResp or folResp
  double growthRespFrac;

  // soil respiration:
  // scalar determining effect of moisture on soil respiration; used as exponent
  // for (W/W_c) fraction in moisture effect calculation
  double soilRespMoistEffect;

  // moisture related:
  // leaf (evaporative) pool rim thickness in mm
  double leafPoolDepth;

  // ****************************************
  // Nitrogen Cycle
  // ****************************************
  // No published source for these, added as part of MAGIC project

  // Initial soil mineral nitrogen pool amount, g C * m^-2 ground area
  double minNInit;

} Params;

#define NUM_PARAMS (sizeof(Params) / sizeof(double))

// Global var
extern Params params;

// the state of the environment
typedef struct Environment {
  ///// From [1] Braswell et al. 2005
  // carbon in plant wood (above-ground + roots)
  // (g C * m^-2 ground area)
  double plantWoodC;
  // carbon in leaves (g C * m^-2 ground area)
  double plantLeafC;
  // carbon in soil (g C * m^-2 ground area)
  double soil;
  // plant available soil water (cm)
  double soilWater;

  ///// From [2] Sacks et al. 2006
  // carbon in litter (g C * m^-2 ground area)
  double litter;
  // snow pack (cm water equiv.)
  double snow;

  ///// From [3] Zobitz et al. (2008)
  // carbon in coarse roots (g C m^-2 ground area)
  double coarseRootC;
  // carbon in fine roots (g C m^-2 ground area)
  double fineRootC;

  ///// From [4] Zobitz (draft)
  // carbon in microbes (g C m^-2 ground area)
  double microbeC;

  ///// MAGIC project
  // soil mineral nitrogen pool (g C m^-2 ground area)
  double minN;
} Envi;

// Global var
extern Envi envi;  // state variables

// fluxes as per-day rates
typedef struct FluxVars {
  // Re: rSoil vs maintRespiration
  // When microbes are in effect, maintResp is the respiration term for the
  // microbes, and rSoil is calculated taking maintResp into account (which
  // seems fine).
  // However, when microbes are off, maintResp and rSoil are treated as the
  // same, which makes _some_ sense (and maintResp is calculated as
  // heterotrophic soil resp is described in [1]), but the use in the code is
  // confusing. Also, the description of maintResp (before this update) only
  // mentions the microbe case, furthering the confusion. For microbes off, I
  // would have expected maintResp to be 0, and rSoil to be calc'd as maintResp
  // is now.
  // The GROWTH_RESP flag may also play in here, as when that is on, we split
  // heterotrophic respiration into growth and maintenance resp. This means
  // maintResp and rSoil should not be the same (microbes off), so again -
  // confusing in the code. Also: are we handling all the permutations of
  // GROWTH_RESP and MICROBES correctly?
  // It's possible that better descriptions of these terms here, and more
  // comments in the code are all that is needed.

  // ****************************************
  // Fluxes from [1] Braswell et al. (2005)
  //  - fluxes tracked as part of modeling from [1]

  // GROSS photosynthesis (g C * m^-2 ground area * day^-1)
  double photosynthesis;
  // Leaf fall (g C * m^-2 ground area * day^-1)
  double leafLitter;
  // Wood flux to litter (g C * m^-2 ground area * day^-1)
  double woodLitter;
  // vegetation respiration (g C * m^-2 ground area * day^-1)
  double rVeg;
  // soil respiration (g C * m^-2 ground area * day^-1)
  double rSoil;
  // Liquid precipitation (cm water * day^-1)
  double rain;
  // Soil transpiration (cm water * day^-1)
  double transpiration;
  // drainage from soil out of system (cm water * day^-1)
  double drainage;

  // ****************************************
  // Fluxes from [2] Sacks et al. (2006)
  //  - fluxes tracked as part of modeling from [2]

  // litter fluxes
  // litter turned into soil (g C * m^-2 ground area * day^-1)
  double litterToSoil;
  // respired by litter (g C * m^-2 ground area * day^-1)
  double rLitter;

  // snow fluxes
  // snow addded (cm water equiv. * day^-1)
  double snowFall;
  // snow removed by melting (cm water equiv. * day^-1)
  double snowMelt;
  // snow removed by sublimation (cm water equiv. * day^-1)
  double sublimation;

  // more complex soil moisture system fluxes
  // rain that's intercepted and immediately evaporated (cm water * day^-1)
  double immedEvap;
  // water entering soil that goes directly to drainage (out of system)
  // (cm water * day^-1)
  double fastFlow;
  // evaporation from top of soil (cm water * day^-1)
  double evaporation;

  // ****************************************
  // Fluxes from [3] Zobitz et al. (2008)
  //  - fluxes tracked as part of modeling from [3]

  // Loss rate of fine roots (turnover + exudation)
  double fineRootLoss;
  // Loss rate of coarse roots (turnover + exudation)
  double coarseRootLoss;
  // Creation rate of fine roots
  double fineRootCreation;
  // Creation rate of coarse roots
  double coarseRootCreation;
  // Coarse root respiration
  double rCoarseRoot;
  // Fine root respiration
  double rFineRoot;

  // ****************************************
  // Fluxes from [4] Zobitz (draft)
  //  - fluxes tracked as part of modeling from [4]

  // Microbes [3]
  // microbes on: microbial maintenance respiration rate
  // microbes off: equivalent to rSoil, calc'd as described in [1], eq (A20)
  // (g C m-2 ground area day^-1)
  double maintRespiration;
  // Flux that microbes remove from soil (mg C g soil day)
  // TBD I highly doubt those units; this is calc'd as
  //     (g C * m-2) * (day-1) * (unitless terms)
  double microbeIngestion;
  // Exudates into the soil
  double soilPulse;

  // ****************************************
  // Fluxes from other sources, provenance TBD
  //

  // leaf creation term as determined by growing season boundaries (as in [1])
  // and NPP (as in [2])
  // C transferred from wood to leaves (g C * m^-2 ground area * day^-1)
  double leafCreation;
  // wood creation term, dependent on NPP similar to leaf creation, but
  // provenance TBD (g C * m^-2 ground area * day^-1)
  double woodCreation;

  // ****************************************
  // Fluxes for event handling
  // Note: this has no published reference source
  //

  // plantLeafC addition
  double eventLeafC;
  // plantWoodC addition
  double eventWoodC;
  // plantFineRootC addition
  double eventFineRootC;
  // plantCoarseRootC addition
  double eventCoarseRootC;
  // irrigation water that is immediately evaporated
  double eventEvap;
  // irrigation water that goes to the soil
  double eventSoilWater;
  // carbon added to litter pool (if used) or soil pool (if not)
  double eventLitterC;
} Fluxes;

// Global var
extern Fluxes fluxes;

typedef struct TrackerVars {  // variables to track various things
  // g C * m^-2 taken up in this time interval; GROSS photosynthesis
  double gpp;
  // g C * m^-2 respired in this time interval
  double rtot;
  // g C * m^-2 autotrophic resp. in this time interval
  double ra;
  // g C * m^-2 heterotrophic resp. in this time interval
  double rh;
  // g C * m^-2 taken up in this time interval
  double npp;
  // g C * m^-2 given off in this time interval
  double nee;
  // g C * m^-2 taken up, year to date: GROSS photosynthesis
  double yearlyGpp;
  // g C * m^-2 respired, year to date
  double yearlyRtot;
  // g C * m^-2 autotrophic resp., year to date
  double yearlyRa;
  // g C * m^-2 heterotrophic resp., year to date
  double yearlyRh;
  // g C * m^-2 taken up, year to date
  double yearlyNpp;
  // g C * m^-2 given off, year to date
  double yearlyNee;
  // g C * m^-2 taken up, to date: GROSS photosynthesis
  double totGpp;
  // g C * m^-2 respired, to date
  double totRtot;
  // g C * m^-2 autotrophic resp., to date
  double totRa;
  // g C * m^-2 heterotrophic resp., to date
  double totRh;
  // g C * m^-2 taken up, to date
  double totNpp;
  // g C * m^-2 given off, to date
  double totNee;
  // cm water evaporated/sublimated (sublimed???) or transpired in this time
  // step
  double evapotranspiration;
  // mean fractional soil wetness (soilWater/soilWHC) over this time step
  // (linear mean: mean of wetness at start of time step and wetness at end of
  // time step)
  double soilWetnessFrac;

  // g C m-2 of root respiration
  double rRoot;
  // Soil respiration (microbes+root)
  double rSoil;

  // Wood and foliar respiration
  double rAboveground;
  // g C * m^-2 litterfall, year to date: SUM litter
  double yearlyLitter;

  // g C * m^-2 wood creation
  double woodCreation;

} Trackers;

// Global var
extern Trackers trackers;

typedef struct PhenologyTrackersStruct {
  // variables to track each year's phenological development. Only used in
  // leafFluxes function, but made global so can be initialized dynamically,
  // based on day of year of first climate record

  // have we done leaf growth at start of growing season this year? (0 = no,
  // 1 = yes)
  int didLeafGrowth;
  // have we done leaf fall at end of growing season this year? (0 = no,
  // 1 = yes)
  int didLeafFall;
  // year of previous time step, for tracking when we hit a new calendar year
  int lastYear;
} PhenologyTrackers;

// Global var
extern PhenologyTrackers phenologyTrackers;

#endif  // SIPNET_STATE_H

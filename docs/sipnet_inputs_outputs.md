# SIPNET Input and Output Parameters

## Table of Contents

1. [Run-time Parameters](#1-run-time-parameters)
   - [Initial state values](#initial-state-values)
   - [Photosynthesis parameters](#photosynthesis-parameters)
   - [Phenology-related parameters](#phenology-related-parameters)
   - [Autotrophic respiration parameters](#autotrophic-respiration-parameters)
   - [Soil respiration parameters](#soil-respiration-parameters)
   - [Moisture-related parameters](#moisture-related-parameters)
   - [Quality model parameters](#quality-model-parameters)
2. [Compile-time parameters](#2-compile-time-parameters)
3. [Climate driver inputs](#3-climate-driver-inputs)
4. [Outputs](#4-outputs)

## 1. Run-time Parameters
   
Model parameters that can change from one run to the next. These include initializations of state, allocations, and biophysical rate constants.  
 

### Initial state values

|  | Parameter Name  | Definition  | Units  | notes |
| :---- | :---- | :---- | :---- | :---- |
| 1 | plantWoodInit | Initial wood carbon | g C * m^-2 ground area | above-ground + roots |
| 2 | laiInit | initial leaf area | m^2 leaves * m^-2 ground area | multiply by leafCSpWt to get initial plant leaf C |
| 3 | litterInit | Initial litter carbon | g C * m^-2 ground area |   |
| 4 | soilInit | Initial soil carbon | g C * m^-2 ground area |   |
| 5 | litterWFracInit |   | unitless | fraction of litterWHC |
| 6 | soilWFracInit |   | unitless | fraction of soilWHC |
| 7 | snowInit | Initial snow water | cm water equiv. |   |


### Photosynthesis parameters

|  | Parameter Name  | Definition  | Units  | notes |
| :---- | :---- | :---- | :---- | :---- |
| 8 | aMax | max photosynthesis | nmol CO2 * g^-1 leaf * sec^-1 | assuming max. possible par, all intercepted, no temp, water or vpd stress |
| 9 | aMaxFrac | avg. daily aMax as fraction of instantaneous |   |   |
| 10 | baseFolRespFrac | basal foliage resp. rate |   | as % of max. net photosynth. rate |
| 11 | psnTMin | min temp at which net photosynthesis occurs | °C |   |
| 12 | psnTOpt | optimal temp at which net photosynthesis occurs | °C |   |
| 13 | dVpdSlope | used to calculate VPD effect on Psn | dimensionless | dVpd = 1 - dVpdSlope * vpd^dVpdExp |
| 14 | dVpdExp | used to calculate VPD effect on Psn | dimensionless | dVpd = 1 - dVpdSlope * vpd^dVpdExp |
| 15 | halfSatPar | par at which photosynthesis occurs at 1/2 theoretical maximum | Einsteins * m^-2 ground area * day^-1 |   |
| 16 | attenuation | light attenuation coefficient |   |   |

### Phenology-related parameters

|  | Parameter Name  | Definition  | Units  | notes |
| :---- | :---- | :---- | :---- | :---- |
| 17 | leafOnDay | day when leaves appear |   |   |
| 18 | gddLeafOn | with gdd-based phenology, gdd threshold for leaf appearance |   |   |
| 19 | soilTempLeafOn | with soil temp-based phenology, soil temp threshold for leaf appearance |   |   |
| 20 | leafOffDay | day when leaves disappear |   |   |
| 21 | leafGrowth | additional leaf growth at start of growing season | g C * m^-2 ground |   |
| 22 | fracLeafFall | additional fraction of leaves that fall at end of growing season |   |   |
| 23 | leafAllocation | fraction of NPP allocated to leaf growth |   |   |
| 24 | leafTurnoverRate | average turnover rate of leaves | fraction per day | read in as per-year rate |


### Autotrophic respiration parameters

|  | Parameter Name  | Definition  | Units  | notes |
| :---- | :---- | :---- | :---- | :---- |
| 25 | baseVegResp | vegetation maintenance respiration at 0°C | g C respired * g^-1 plant C * day^-1 | read in as per-year rate   only counts plant wood C; leaves handled elsewhere (both above and below-ground: assumed for now to have same resp. rate) |
| 26 | vegRespQ10 | scalar determining effect of temp on veg. resp. |   |   |
| 27 | growthRespFrac | growth resp. as fraction of (GPP - woodResp - folResp) |   |   |
| 28 | frozenSoilFolREff | amount that foliar resp. is shutdown if soil is frozen |   | 0 = full shutdown, 1 = no shutdown |
| 29 | frozenSoilThreshold | soil temperature below which frozenSoilFolREff and frozenSoilEff kick in | °C |   |

### Soil respiration parameters


|  | Parameter Name  | Definition  | Units  | notes |
| :---- | :---- | :---- | :---- | :---- |
| 30 | litterBreakdownRate | rate at which litter is converted to soil / respired at 0°C and max soil moisture |  g C broken down * g^-1 litter C * day^-1 | read in as per-year rate |
| 31 | fracLitterRespired | of the litter broken down, fraction respired (the rest is transferred to soil pool) |   |   |
| 32 | baseSoilResp | soil respiration at 0°C and max soil moisture | g C respired * g^-1 soil C * day^-1 | read in as per-year rate |
| 33 | baseSoilRespCold | soil respiration at 0°C and max soil moisture when tsoil < coldSoilThreshold | g C respired * g^-1 soil C * day^-1 | read in as per-year rate |
| 34 | soilRespQ10 | scalar determining effect of temp on soil resp. |   |   |
| 35 | soilRespQ10Cold | scalar determining effect of temp on soil resp. when tsoil < coldSoilThreshold |   |   |
| 36 | coldSoilThreshold | temp. at which use baseSoilRespCold and soilRespQ10Cold | °C | Not used if SEASONAL_R_SOIL  is 0 |
| 37 | E0 | E0 in Lloyd-Taylor soil respiration function |   | Not used if LLOYD_TAYLOR is 0 |
| 38 | T0 | T0 in Lloyd-Taylor soil respiration function |   | Not used if LLOYD_TAYLOR is 0 |
| 39 | soilRespMoistEffect | scalar determining effect of moisture on soil resp. |   |   |


###  Moisture-related parameters

|  | Parameter Name  | Definition  | Units  | notes |
| :---- | :---- | :---- | :---- | :---- |
| 40 | waterRemoveFrac | fraction of plant available soil water which can be removed in one day	without water stress occurring |   |   |
| 41 | frozenSoilEff | fraction of water that is available if soil is frozen (0 = none available, 1 = all still avail.) |   | if frozenSoilEff = 0, then shut down psn. even if WATER_PSN = 0, if soil is frozen (if frozenSoilEff > 0, it has no effect if WATER_PSN = 0) |
| 42 | wueConst | water use efficiency constant |   |   |
| 43 | litterWHC | litter (evaporative layer) water holding capacity | cm |   |
| 44 | soilWHC | soil (transpiration layer) water holding capacity | cm |   |
| 45 | immedEvapFrac | fraction of rain that is immediately intercepted & evaporated |   |   |
| 46 | fastFlowFrac | fraction of water entering soil that goes directly to drainage |   |   |
| 47 | snowMelt | rate at which snow melts | cm water equiv./°C/day |   |
| 48 | litWaterDrainRate | rate at which litter water drains into lower layer when litter layer fully moisture-saturated | c m water/day |   |
| 49 | rdConst | scalar determining amount of aerodynamic resistance |   |   |
| 50 | rSoilConst1 |   |   | soil resistance = e^(rSoilConst1 - rSoilConst2 * W1) , where W1 = (litterWater/litterWHC) |
| 51 | rSoilConst2 |   |   | soil resistance = e^(rSoilConst1 - rSoilConst2 * W1) , where W1 = (litterWater/litterWHC) |
| 52 | m_ballBerry | slope for the Ball Berry relationship |   |   |
| 53 | leafCSpWt |   | g C * m^-2 leaf area |   |
| 54 | cFracLeaf |   | g leaf C * g^-1 leaf |   |
| 55 | woodTurnoverRate | average turnover rate of woody plant C | fraction per day | read in as per-year rate; leaf loss handled separately |

### Quality model parameters

|  | Parameter Name  | Definition  | Units  | notes |
| :---- | :---- | :---- | :---- | :---- |
| 56 | qualityLeaf | value for leaf litter quality |   |   |
| 57 | qualityWood | value for wood litter quality |   |   |
| 58 | efficiency | conversion efficiency of ingested carbon |   |   |
| 59 | maxIngestionRate | maximum ingestion rate of the microbe | hr-1 |   |
| 60 | halfSatIngestion | half saturation ingestion rate of microbe | mg C g-1 soil |   |
| 61 | totNitrogen | Percentage nitrogen in soil |   |   |
| 62 | microbeNC | microbe N:C ratio | mg N / mg C |   |
| 63 | microbeInit |   | mg C / g soil microbe | initial carbon amount |
| 64 | fineRootFrac | fraction of wood carbon allocated to fine roots |   |   |
| 65 | coarseRootFrac | fraction of wood carbon that is coarse roots |   |   |
| 66 | fineRootAllocation | fraction of NPP allocated to fine roots |   |   |
| 67 | woodAllocation | fraction of NPP allocated to wood |   |   |
| 68 | fineRootExudation | fraction of GPP exuded to the soil |   |   |
| 69 | coarseRootExudation | fraction of NPP exuded to the soil |   |   |
| 70 | fineRootTurnoverRate | turnover of fine roots | per year rate |   |
| 71 | coarseRootTurnoverRate | turnover of coarse roots | per year rate |   |
| 72 | baseFineRootResp | base respiration rate of fine roots | per year rate |   |
| 73 | baseCoarseRootResp | base respiration rate of coarse roots | per year rate |   |
| 74 | fineRootQ10 | Q10 of fine roots |   |   |
| 75 | coarseRootQ10 | Q10 of coarse roots |   |   |
| 76 | baseMicrobeResp | base respiration rate of microbes |   |   |
| 77 | microbeQ10 | Q10 of coarse roots |   |   |
| 78 | microbePulseEff | fraction of exudates that microbes immediately use |   |   |

## 2. Compile-time parameters

| CSV_OUTPUT 0 | 0 | output .out file as a CSV file |
| :---- | :---- | :---- |
| ALTERNATIVE_TRANS 0 | 0 | do we want to impliment alternative transpiration? |
| BALL_BERRY 0 | 0 | impliment a Ball Berry submodel to calculate gs from RH, CO2 and A |
| PENMAN_MONTEITH_TRANS 0 | 0 | impliment a transpiration calculation based on the Penman-Monteith Equation |
| GROWTH_RESP 0 | 0 | explicitly model growth resp., rather than including with maint. resp. |
| LLOYD_TAYLOR 0 | 0 | use Lloyd-Taylor model for soil respiration, in which temperature sensitivity decreases at higher temperatures? |
| SEASONAL_R_SOIL 0 && !LLOYD_TAYLOR | 0 | use different parameters for soil resp. (baseSoilResp and soilRespQ10) when tsoil < (some threshold)? |
| WATER_PSN 1 | 1 | does soil moisture affect photosynthesis? |
| WATER_HRESP 1 | 1 | does soil moisture affect heterotrophic respiration? |
| DAYCENT_WATER_HRESP 0 && WATER_HRESP | 0 | use DAYCENT soil moisture function? |
| MODEL_WATER 1 | 1 | do we model soil water (and ignore soilWetness)? |
| COMPLEX_WATER 1 && MODEL_WATER | 1 | do we use a more complex water submodel? (model evaporation as well as transpiration) |
| LITTER_WATER 0 && (COMPLEX_WATER) | 0 | do we have a separate litter water layer, used for evaporation? |
| LITTER_WATER_DRAINAGE 1 && (LITTER_WATER) | 0 | does water from the top layer drain down into bottom layer even if top layer not overflowing? |
| SNOW (1 \|\| (COMPLEX_WATER)) && MODEL_WATER | 1 | keep track of snowpack, rather than assuming all precip. is liquid |
| GDD 0 | 0 | use GDD to determine leaf growth? (note: mutually exclusive with SOIL_PHENOL) |
| SOIL_PHENOL 0 && !GDD | 0 | use soil temp. to determine leaf growth? (note: mutually exclusive with GDD) |
| LITTER_POOL 0 | 0 | have extra litter pool, in addition to soil c pool |
| SOIL_MULTIPOOL 0 && !LITTER_POOL | 0 | do we have a multipool approach to model soils? |
| NUMBER_SOIL_CARBON_POOLS 3 | 3 | number of pools we want to have. Equal to 1 if SOIL_MULTIPOOL is 0 |
| SOIL_QUALITY 0 && SOIL_MULTIPOOL | 0 | do we have a soil quality submodel? |
| MICROBES 0 && !SOIL_MULTIPOOL | 0 | do we utilize microbes. This will only be an option if SOIL_MULTIPOOL is 0 and MICROBES is 1 |
| STOICHIOMETRY 0 && MICROBES | 0 | do we utilize stoichometric considerations for the microbial pool? |
| ROOTS 0 | 0 | do we model root dynamics? |
| MODIS 0 | 0 | do we use modis FPAR data to constrain GPP? |
| C_WEIGHT 12.0 | 12 | molecular weight of carbon |
| MEAN_NPP_DAYS 5 | 5 | over how many days do we keep the running mean |
| MEAN_NPP_MAX_ENTRIES | MEAN_NPP_DAYS*50 | assume that the most pts we can have is two per hour |
| MEAN_GPP_SOIL_DAYS 5 | 5 | over how many days do we keep the running mean |
| MEAN_GPP_SOIL_MAX_ENTRIES | MEAN_GPP_SOIL_DAYS*50 | assume that the most pts we can have is one per hour |
| LAMBDA | 2501000 | latent heat of vaporization (J/kg) |
| LAMBDA_S | 2835000 | latent heat of sublimation (J/kg) |
| RHO | 1.3 | air density (kg/m^3) |
| CP | 1005. | specific heat of air (J/(kg K)) |
| GAMMA | 66 | psychometric constant (Pa/K) |
| E_STAR_SNOW | 0.6 | approximate saturation vapor pressure at 0°C (kPa) |

## 3. Climate driver inputs

For each step of the model, for each location, the following inputs are needed.  

| 1 | loc | spatial location index |   | maps to param-spatial file |
| :---- | :---- | :---- | :---- | :---- |
| 2 | year   | year of start of this timestep |   | e.g. 2010 |
| 3 | day | day of start of this timestep |   | 1 = Jan 1 |
| 4 | time | time of start of this timestep |   hour fraction | e.g. noon = 12.0, midnight = 0.0 |
| 5 | length | length of this timestep | days | allow variable-length timesteps |
| 6 | tair | avg. air temp for this time step | °C |   |
| 7 | tsoil | avg. soil temp for this time step | °C |   |
| 8 | par   | average par for this time step | Einsteins * m^-2 ground area * day^-1 | input is in Einsteins * m^-2 ground area, summed over entire time step |
| 9 | precip | total precip. for this time step | cm water equiv. - either rain or snow   | input is in mm |
| 10 | vpd | average vapor pressure deficit | kPa | input is in Pa |
| 11 | vpdSoil | average vapor pressure deficit between soil and air | kPa | input is in Pa ; differs from vpd in that saturation vapor pressure calculated using Tsoil rather than Tair |
| 12 | vPress | average vapor pressure in canopy airspace | kPa | input is in Pa |
| 13 | wspd | avg. wind speed | m/s |   |
| 14 | soilWetness | fractional soil wetness | fraction of saturation - between 0 and 1 | Not used if MODEL_WATER is 1 |

   
   
## 4. Outputs

For each step of the model, for each location, the following outputs are generated.  
 
|  | Parameter Name  | Definition  | Units  | 
| :---- | :---- | :---- | :---- |
| 1 | loc | spatial location index |   |   
| 2 | year | year of start of this timestep |  |
| 3 | day | day of start of this timestep | |
| 4 | time | time of start of this timestep |  |
| 5 | plantWoodC | carbon in wood | g C/m^2 |
| 6 | plantLeafC | carbon in leaves | g C/m^2 |
| 7 | soil | carbon in mineral soil | g C/m^2 |
| 8 | microbeC | carbon in soil microbes |   |
| 9 | coarseRootC | carbon in coarse roots |   |
| 10 | fineRootC | carbon in fine roots |   |
| 11 | litter | carbon in litter | g C/m^2 |
| 12 | litterWater | moisture in litter layer | cm |
| 13 | soilWater | moisture in soil | cm |
| 14 | soilWetnessFrac | moisture in soil as fraction |   |
| 15 | snow | snow water | cm |
| 16 | npp | net primary production |   |
| 17 | nee | net ecosystem production |   |
| 18 | cumNEE | cumulative nee |   |
| 19 | gpp | gross ecosystem production |   |
| 20 | rAboveground | plant respiration above ground |   |
| 21 | rSoil | soil respiration |   |
| 22 | rRoot | root respiration |   |
| 23 | rtot | total respiration |   |
| 24 | fluxestranspiration | transpiration |   |

   

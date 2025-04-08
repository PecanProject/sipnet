---
format:
  html:
    engine: katex
  pdf:
    geometry: margin=0.5in
    header-includes:
      - \usepackage{longtable}
      - \usepackage{amsmath}
---

# Input and Output Parameters (DRAFT)

Note: this is a work in progress draft. Not all parameters listed will be used in the CCMMF formulation of the model. The "Notation" section should be consistent with model equations, some of the mathematical symbols in the tables may not be.

Numbered items are cross-referenced with original documentation.

## Table of Contents

- [Input and Output Parameters (DRAFT)](#input-and-output-parameters-draft)
  - [Table of Contents](#table-of-contents)
  - [Notation](#notation)
    - [Variables (Pools, Fluxes, and Parameters)](#variables-pools-fluxes-and-parameters)
    - [Subscripts (Temporal, Spatial, or Contextual Identifiers)](#subscripts-temporal-spatial-or-contextual-identifiers)
  - [Run-time Parameters](#run-time-parameters)
    - [Initial state values](#initial-state-values)
    - [Litter Quality Parameters](#litter-quality-parameters)
    - [Photosynthesis parameters](#photosynthesis-parameters)
    - [Phenology-related parameters](#phenology-related-parameters)
    - [Allocation parameters](#allocation-parameters)
    - [Autotrophic respiration parameters](#autotrophic-respiration-parameters)
    - [Soil respiration parameters](#soil-respiration-parameters)
    - [Nitrogen Cycle Parameters](#nitrogen-cycle-parameters)
    - [Methane parameters](#methane-parameters)
    - [Moisture-related parameters](#moisture-related-parameters)
    - [Tree physiological parameters](#tree-physiological-parameters)
  - [Compile-time parameters](#compile-time-parameters)
  - [Drivers](#drivers)
    - [Climate](#climate)
      - [Examples of climate files:](#examples-of-climate-files)
    - [Agronomic Events](#agronomic-events)
      - [Fertilization Events](#fertilization-events)
      - [Tillage Events](#tillage-events)
      - [Planting Events](#planting-events)
      - [Harvest Events](#harvest-events)
      - [Example of events file:](#example-of-events-file)
  - [Outputs](#outputs)

## Notation

### Variables (Pools, Fluxes, and Parameters)

| Symbol         | Description                                                                 | 
|----------------|-----------------------------------------------------------------------------| 
| $C$        | Carbon pool                                                                 |
| $N$       | Nitrogen pool                                                               |
| $CN$       | Carbon-to-Nitrogen ratio                                                    |
| $W$        | Water pool or content                                                       |
| $R$        | Respiration flux                                                            |
| $A$        | Photosynthesis rate (net assimilation)                                      |
| $T$        | Temperature                                                                 |
| $K$        | Rate constant (e.g., for decomposition or respiration)                      |
| $LAI$      | Leaf Area Index                                                             |
| $PAR$      | Photosynthetically Active Radiation                                         |
| $GPP$      | Gross Primary Production                                                    |
| $NPP$      | Net Primary Production                                                      |
| $NEE$      | Net Ecosystem Exchange                                                      |
| $VPD$      | Vapor Pressure Deficit                                                      |
| $ET$       | Evapotranspiration                                                          |
| $Q_{10}$   | Temperature sensitivity coefficient                                         |
| $f$        | Fraction                                                                    |
| $F$        | Flux of carbon, nitrogen, or water                                          |
| $D$        | Dependency or Damping Function                                              |
| $N$        | Nitrogen                                                                    |
| $C$        | Carbon                                                                      |
| $\alpha$   | Allocation fraction                                                         |
| $k$        | Scaling factor |

### Subscripts (Temporal, Spatial, or Contextual Identifiers)

| Subscript      | Description                                                                 |
|----------------|-----------------------------------------------------------------------------|
| $X_0$       | Initial value, default value, state at time zero                            |
| $X_t$       | Value at time $t$    |
| $X_d$       | Daily value              |
| $X_\text{max}$ | Maximum value (e.g., temperature or rate)                                |
| $X_\text{min}$ | Minimum value (e.g., temperature or rate)                                |
| $X_\text{opt}$ | Optimal value (e.g., temperature or rate)                                |
| $X_\text{avg}$ | Average value (e.g., over a timestep or spatial area)                   |
| $X_\text{leaf}$ | leaf pools or fluxes                                      |
| $X_\text{wood}$ | wood pools or fluxes                                      |
| $X_\text{root}$ | root pool |
| $X_\text{fine root}$ | fine root pool    |
| $X_\text{coarse root}$ | coarse root pool |
| $X_\text{soil}$ | soil pools or processes                                   |
| $X_\text{litter}$ | litter pools or processes                               |
| $X_\text{veg}$ | vegetation processes (general)                            |
| $X_\text{resp}$ | respiration processes                                     |
| $X_\text{dec}$ | decomposition processes                                    |
| $X_\text{vol}$ | volatilization processes                                   |
| $X_\text{VPD}$ | vapor pressure deficit                                     |
| $X_\text{org}$ | organic forms                                             |
| $X_\text{mineral}$ | mineral forms                                         |
| $X_{\text{anaer}}$ | anaerobic soil conditions                                |

Subscripts may be used in combination, e.g. $X_{\text{soil,mineral},0}$.

## Run-time Parameters

Run-time parameters can change from one run to the next, or when the model is stopped and restarted. These include initial state values and parameters related to plant physiology, soil physiology, and biogeochemical cycling.

### Initial state values

|   | Symbol                                   | Parameter Name  | Definition                                | Units                          | notes                                             |
| - | ---------------------------------------- | --------------- | ----------------------------------------- | ------------------------------ | ------------------------------------------------- |
| 1 | $C_{\text{wood},0}$                    | plantWoodInit   | Initial wood carbon                       | $\text{g C} \cdot \text{m}^{-2} \text{ ground area}$        | above-ground + roots                              |
| 2 | $LAI_0$                                  | laiInit         | Initial leaf area                         | m^2 leaves \* m^-2 ground area | multiply by SLW to get initial plant leaf C: $C_{\text{leaf},0} = LAI_0 \cdot SLW$ |
 |
| 3 | $C_{\text{litter},0}$                  | litterInit      | Initial litter carbon                     | $\text{g C} \cdot \text{m}^{-2} \text{ ground area}$        |                                                   |
| 4 | $C_{\text{soil},0}$                    | soilInit        | Initial soil carbon                       | $\text{g C} \cdot \text{m}^{-2} \text{ ground area}$        |                                                   |
| 5 | $W_{\text{litter},0}$                  | litterWFracInit |                                           | unitless                       | fraction of litterWHC                             |
| 6 | $W_{\text{soil},0}$                              | soilWFracInit   |                                           | unitless                       | fraction of soilWHC                               |
|   | $N_{\text{soil},0}$                              |                 | Initial soil organic nitrogen content     | g N m$^{-2}$                 |                                                   |
|   | ${CH_4}_{\text{soil},0}$                         |                 | Initial methane concentration in the soil | g C m$^{-2}$                 |                                                   |
|   | ${N_2O}_{\text{soil},0}$                         |                 | Nitrous oxide concentration in the soil   | g N m$^{-2}$                 |                                                   |
|   | $C_{\text{fine root},0}$    |  fineRootFrac    | Initial fine root carbon                  | g C m$^{-2}$ | |
|   | $C_{\text{coarse root},0}$  |   coarseRootFrac | Initial coarse root carbon                | g C m$^{-2}$                 |                                                   |
         |                                                   |

<!--not used in CCMMF

| 7 |                                          | snowInit        | Initial snow water                        | cm water equiv.                |                                                   |
|   |                                          | microbeInit     |                                           |                                |                                                   |

-->

<!--if separating N_min into NH4 and NO3

### Initial state values 

|   | Symbol                                   | Parameter Name  | Definition                                | Units                          | notes                                             |
| - | ---------------------------------------- | --------------- | ----------------------------------------- | ------------------------------ | ------------------------------------------------- |
|   | ${NH_4}_{\text{soil},0}$                         |                 | Initial soil ammonium content             | g N m$^{-2}$                 |                                                   |
|   | ${NO_3}_{\text{soil},0}$                         |                 | Initial soil nitrate content              | g N m$^{-2}$                 |                                                   |

-->

### Litter Quality Parameters

|     | Symbol                      | Name | Description                              | Units | Notes                    |
| --- | --------------------------- | ---- | ---------------------------------------- | ----- | ------------------------ |
|     | $CN_{\textrm{litter}}$      |      | Carbon to Nitrogen ratio of litter       |       |                          |
|     | $CN_{\textrm{wood}}$        |      | Carbon to Nitrogen ratio of wood         |       | CN_coarse_root = CN_wood |
|     | $CN_{\textrm{leaf}}$        |      | Carbon to Nitrogen ratio of leaves       |       |                          |
|     | $CN_{\textrm{fine root}}$   |      | Carbon to Nitrogen ratio of fine roots   |       |                          |
|     | $CN_{\textrm{coarse root}}$ |      | Carbon to Nitrogen ratio of coarse roots |       |                          |
|     | $k_\textit{CN}$             |      | Decomposition CN scaling parameter       |       |                          |

### Photosynthesis parameters

|     | Symbol                        | Parameter Name  | Definition                                                           | Units                                                                        | notes                                                                     |
| --- | ----------------------------- | --------------- | -------------------------------------------------------------------- | ---------------------------------------------------------------------------- | ------------------------------------------------------------------------- |
| 8   | $A_{\text{max}}$              | aMax            | Maximum net CO2 assimilation rate                                    | $\text{nmol CO}_2 \cdot \text{g}^{-1} \cdot \text{leaf} \cdot \text{s}^{-1}$ | assuming max. possible PAR, all intercepted, no temp, water or VPD stress |
| 9   | $f_{A_{\text{max},d}}$        | aMaxFrac        | avg. daily aMax as fraction of instantaneous                         | fraction                                                                     | Avg. daily max photosynthesis as fraction of $A_{\text{max}}$             |
| 10  | $R_\text{leaf,opt}$           | baseFolRespFrac | basal Foliar maintenance respiration as fraction of $A_{\text{max}}$ | fraction                                                                     |                                                                           |
| 11  | $T_{\text{min}}$              | psnTMin         | Minimum temperature at which net photosynthesis occurs               | $^{\circ}\text{C}$                                                           |                                                                           |
| 12  | $T_{\text{opt}}$              | psnTOpt         | Optimum temperature at which net photosynthesis occurs               | $^{\circ}\text{C}$                                                           |                                                                           |
| 13  | $K_\text{VPD}$                | dVpdSlope       | Slope of VPD–photosynthesis relationship                             | $kPa^{-1}$                                                                   | dVpd = 1 - dVpdSlope \* vpd^dVpdExp                                       |
| 14  | $K_{\text{VPD}},{\text{exp}}$ | dVpdExp         | Exponent used to calculate VPD effect on Psn                         | dimensionless                                                                | dVpd = 1 - dVpdSlope \* vpd^dVpdExp                                       |
| 15  | $\text{PAR}_{1/2}$            | halfSatPar      | Half saturation point of PAR–photosynthesis relationship             | $m^{-2}$\ ground area $\cdot$ day$^{-1}$                                     | PAR at which photosynthesis occurs at 1/2 theoretical maximum             |
| 16  | $k$                           | attenuation     | Canopy PAR extinction coefficient                                    |                                                                              |                                                                           |

### Phenology-related parameters

|    | Symbol             | Parameter Name   | Definition                                                              | Units                              | notes                                          |
| -- | ------------------ | ---------------- | ----------------------------------------------------------------------- | ---------------------------------- | ---------------------------------------------- |
| 17 | $D_{\text{on}}$  | leafOnDay        | Day of year when leaves appear                                          | day of year                        |                                                |
| 18 |                    | gddLeafOn        | with gdd-based phenology, gdd threshold for leaf appearance             |                                    |                                                |
| 19 |                    | soilTempLeafOn   | with soil temp-based phenology, soil temp threshold for leaf appearance |                                    |                                                |
| 20 | $D_{\text{off}}$ | leafOffDay       | Day of year for leaf drop                                               |                                    |                                                |
| 21 |                    | leafGrowth       | additional leaf growth at start of growing season                       | $\text{g C} \cdot \text{m}^{-2} \text{ ground}$                 |                                                |
| 22 |                    | fracLeafFall     | additional fraction of leaves that fall at end of growing season        |                                    |                                                |
| 23 | $\alpha_\text{leaf}$       | leafAllocation   | fraction of NPP allocated to leaf growth                                |                                    |                                                |
| 24 | $K_{leaf}$    | leafTurnoverRate | average turnover rate of leaves                                         | fraction per day                   | read in as per-year rate                       |
|    | $L_{\text{max}}$ |                  | Maximum leaf area index obtained                                        | $\text{m}^2 \text{ leaf } \text{m}^{-2} \text{ ground}$ | ? from Braswell et al 2005; can't find in code |


### Allocation parameters

|    | Symbol                    | Parameter Name      | Definition                                     | Units | notes              |
|----|---------------------------|---------------------|------------------------------------------------|-------|--------------------|
| 64 |                           | fineRootFrac        | fraction of wood carbon allocated to fine root |       |                    |
| 65 |                           | coarseRootFrac      | fraction of wood carbon that is coarse root    |       |                    |
| 66 | $\alpha_\text{fine root}$ | fineRootAllocation  | fraction of NPP allocated to fine roots        |       |                    |
| 67 | $\alpha_\text{wood}$      | woodAllocation      | fraction of NPP allocated to wood              |       |                    |
<!--| 68 |                           | fineRootExudation   | fraction of GPP exuded to the soil             |       | Pulsing parameters |
| 69 |                           | coarseRootExudation | fraction of NPP exuded to the soil             |       | Pulsing parameters |
-->

### Autotrophic respiration parameters

|    | Symbol      | Parameter Name      | Definition                                                               | Units                                              | notes                                                                                                                                              |
| -- | ----------- | ------------------- | ------------------------------------------------------------------------ | -------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------- |
| 25 | $R_{\text{a,wood},0}$     | baseVegResp         | Wood maintenance respiration rate at $0^\circ C$                       | g C respired \* g$^{-1}$ plant C \* day$^{-1}$ | read in as per-year rate only counts plant wood C; leaves handled elsewhere (both above and below-ground: assumed for now to have same resp. rate) |
| 26 | $Q_{10v}$ | vegRespQ10          | Vegetation respiration Q10                                               |                                                    | Scalar determining effect of temp on veg. resp.                                                                                                    |
| 27 |             | growthRespFrac      | growth resp. as fraction of ($GPP - R_\text{a,wood} - R_\text{a,leaf}$) |                                                        |
| 28 |             | frozenSoilFolREff   | amount that foliar resp. is shutdown if soil is frozen                   |                                                    | 0 = full shutdown, 1 = no shutdown                                                                                                                 |
| 29 |             | frozenSoilThreshold | soil temperature below which frozenSoilFolREff and frozenSoilEff kick in | °C                                                 |                                                                                                                                                    |                                                       |                                                                                                                                               |
| 72 |                   | baseFineRootResp       | base respiration rate of fine roots                | $\text{y}^{-1}$                               |per year rate         | 
| 73 |                   | baseCoarseRootResp     | base respiration rate of coarse roots              | $\text{y}^{-1}$                               |per year rate         |


### Soil respiration parameters

|    | Symbol      | Parameter Name      | Definition                                                                          | Units                                             | notes                                                 |
| -- | ----------- | ------------------- | ----------------------------------------------------------------------------------- | ------------------------------------------------- | ----------------------------------------------------- |
| 30 |   $K_\text{litter}$          | litterBreakdownRate | rate at which litter is converted to soil / respired at 0°C and max soil moisture   | g C broken down \* g^-1 litter C \* day^-1        | read in as per-year rate                              |
| 31 |             | fracLitterRespired  | of the litter broken down, fraction respired (the rest is transferred to soil pool) |                                                   |                                                       |
| 32 | $K_{dec}$     | baseSoilResp        | Soil respiration rate at $0 ^{\circ}\text{C}$ and moisture saturated soil          | g C respired \* g$^{-1}$ soil C \* day$^{-1}$ | read in as per-year rate                              |
| 33 |             | baseSoilRespCold    | soil respiration at 0°C and max soil moisture when tsoil < coldSoilThreshold        | g C respired \* g$^{-1}$ soil C \* day$^{-1}$ | read in as per-year rate                              |
| 34 | $Q_{10s}$ | soilRespQ10         | Soil respiration Q10                                                                |                                                   | scalar determining effect of temp on soil respiration |
| 35 |             | soilRespQ10Cold     | scalar determining effect of temp on soil resp. when tsoil < coldSoilThreshold      |                                                   |                                                       |
| 36 |             | coldSoilThreshold   | temp. at which use baseSoilRespCold and soilRespQ10Cold                             | °C                                                | Not used if SEASONAL\_R\_SOIL is 0                    |
| 37 |             | E0                  | E0 in Lloyd-Taylor soil respiration function                                        |                                                   | Not used if LLOYD\_TAYLOR is 0                        |
| 38 |             | T0                  | T0 in Lloyd-Taylor soil respiration function                                        |                                                   | Not used if LLOYD\_TAYLOR is 0                        |
| 39 |             | soilRespMoistEffect | scalar determining effect of moisture on soil resp.                                 |                                                   |                                                       |
|    |             | baseMicrobeResp     |                                                                                     |                                                   |                                                       |
|    |             |                     |                                                                                     |                                                   |                                                       |

- $R_{dec}$: Rate of decomposition $(\text{day}^{-1})$ 
- $Q_{10dec}$: Temperature coefficient for $R_{dec}$ (unitless)

### Nitrogen Cycle Parameters

<!--
- $K_{nitr}$: Rate constant for nitrification (day$^{-1}$)
- $K_{denitr}$: Rate constant for denitrification (day$^{-1}$)
-->

- $K_{n,vol}$: Rate constant for volatilization (day-1)
- $f_{N2O_{vol}}$: Fraction of volatilization leading to N2O production
- $R_{min}$: Rate of mineralization (day-1)
- $I_\text{N limit}$: Indicator for nitrogen limitation

<!--- $f_{N2O_{nitr}}$: Fraction of nitrification leading to N$_2$O production
- $f_{N2O_{denitr}}$: Fraction of denitrification leading to N$_2$O production
-->

<!--
- $R_{nitr}$: Rate of nitrification (day$^{-1}$)
- $R_{denitr}$: Rate of denitrification (day$^{-1}$)
- $Q_{10nitr}$: Temperature coefficient for $R_{nitr}$ (unitless)
- $Q_{10denitr}$: Temperature coefficient for $R_{denitr}$ (unitless)
-->

### Methane parameters

- $R_{meth}$: Rate of methane production $(\text{day}^{-1})$
- $K_{meth}$: Rate constant for methane production under anaerobic conditions $(\text{day}^{-1})$
- $K_{methox}$: Rate constant, methane oxidation $(\text{day}^{-1})$

### Moisture-related parameters

|    | Symbol     |Parameter Name    | Definition                                                                                            | Units                  | notes                                                                                                                                          |
| -- | ---------- | ----------------- | ----------------------------------------------------------------------------------------------------- | ---------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------- |
| 40 |   $f_{\text{trans,avail}}$ | waterRemoveFrac   | fraction of plant available soil water which can be removed in one day by transpiration without water stress occurring |                        |                                                                                      |
| new |  $f_\text{drain,0}$ | waterDrainFrac    | fraction of plant available soil water which can be removed in one day by drainage    |     $d^{-1}$ | default 1 for well drained soils |
| 41 |            | frozenSoilEff     | fraction of water that is available if soil is frozen (0 = none available, 1 = all still avail.)      |                        | if frozenSoilEff = 0, then shut down psn. even if WATER\_PSN = 0, if soil is frozen (if frozenSoilEff > 0, it has no effect if WATER\_PSN = 0) |
| 42 |            | wueConst          | water use efficiency constant                                                                         |                        |                                                                                                                                                |
| 43 |            | litterWHC         | litter (evaporative layer) water holding capacity                                                     | cm                     |                                                                                                                                                |
| 44 |            | soilWHC           | soil (transpiration layer) water holding capacity                                                     | cm                     |                                                                                                                                                |
| 45 |   $f_\text{intercept}  | immedEvapFrac     | fraction of rain that is immediately intercepted & evaporated                                         |                        |                                                                                                                                                |
| 46 |            | fastFlowFrac      | fraction of water entering soil that goes directly to drainage                                        |                        |                                                                                                                                                |
|    | $k_\text{SOM,drain}$ |
| 47 |            | snowMelt          | rate at which snow melts                                                                              | cm water equivavlent per degree Celsius per day |                                                                                                                                                |
| 48 |            | litWaterDrainRate | rate at which litter rains into lower layer when litter layer fully moisture-saturated         | cm water/day          |                                                                                                                                                |
| 49 |            | rdConst           | scalar determining amount of aerodynamic resistance                                                   |                        |                                                                                                                                                |
| 50 |            | rSoilConst1       |                                                                                                       |                        | soil resistance = e^(rSoilConst1 - rSoilConst2 \* W1) , where W1 = (litterWater/litterWHC)                                                     |
| 51 |            | rSoilConst2       |                                                                                                       |                        | soil resistance = e^(rSoilConst1 - rSoilConst2 \* W1) , where W1 = (litterWater/litterWHC)                                                     |
| 52 |            | m\_ballBerry      | slope for the Ball Berry relationship                                                                 |                        |                                                                                                                                                |


### Tree physiological parameters

|    | Symbol       | Parameter Name    | Definition                                                                                            | Units                  | notes                                                  |
| -- | ------------ | ----------------- | ----------------------------------------------------------------------------------------------------- | ---------------------- | ----------------------------------------------------- |
| 53 | $SLW$        | leafCSpWt         |                                                                                                       | g C * m^-2 leaf area   |                                                        |
| 54 | $C_{frac}$   | cFracLeaf         |                                                                                                       | g leaf C * g^-1 leaf   |                                                        |
| 55 | $K_\text{wood}$        | woodTurnoverRate  | average turnover rate of woody plant C                                                                | $\text{y}^{-1}$       | read in as per-year rate; leaf loss handled separately |
| 70 |    $K_\text{fine root}$               | fineRootTurnoverRate   | turnover of fine roots                             | $\text{y}^{-1}$                              | per year rate         |
| 71 | $K_\text{coarse root}$ | coarseRootTurnoverRate | turnover of coarse roots                           | yr^-1                               |per year rate         |



<!--
### Quality model parameters


|    |  Symbol           | Parameter Name         | Definition                                         | Units                 | notes                         |
| -- | ----------------- |  --------------------- | -------------------------------------------------- | --------------------- | ----------------------------- |
| 56 |                   | qualityLeaf            | value for leaf litter quality                      |                       |                               |
| 57 |                   | qualityWood            | value for wood litter quality                      |                       |                               |
| 58 |                   | efficiency             | conversion efficiency of ingested carbon           |                       | Microbe & Stoichiometry model |
| 59 |                   | maxIngestionRate       | maximum ingestion rate of the microbe              | hr-1                  | Microbe & Stoichiometry model |
| 60 |                   | halfSatIngestion       | half saturation ingestion rate of microbe          | mg C g-1 soil         | Microbe & Stoichiometry model |
| 61 |                   | totNitrogen            | Percentage nitrogen in soil                        |                       | Microbe & Stoichiometry model |
| 62 |                   | microbeNC              | microbe N:C ratio                                  | mg N / mg C           | Microbe & Stoichiometry model |
| 63 |                   | microbeInit            |                                                    | mg C / g soil microbe | initial carbon amount         |-->



<!-- Not used in CCMMF (two Q10s , soil and veg); no microbes
| 74 |                   | fineRootQ10            | Q10 of fine roots                                  |                       |                               |
| 75 |                   | coarseRootQ10          | Q10 of coarse roots                                |                       |                               |
| 76 |                   | baseMicrobeResp        | base respiration rate of microbes                  |                       |                               |
| 77 |                   | microbeQ10             | Q10 of microbes                                |                       |                               |
| 78 |                   | microbePulseEff        | fraction of exudates that microbes immediately use |                       | Pulsing parameters            |

-->

## Compile-time parameters

| Parameter 0                                    | Default                   | Description                                                                                                     |
| ---------------------------------------------- | ------------------------- | --------------------------------------------------------------------------------------------------------------- |
| CSV\_O                                         | 0                         | output .out file as a CSV file                                                                                  |
| ALTERNATIVE\_TRANS 0                           | 0                         | do we want to implement alternative transpiration?                                                              |
| BALL\_BERRY 0                                  | 0                         | implement a Ball Berry submodel to calculate gs from RH, CO2 and A                                              |
| PENMAN\_MONTEITH\_TRANS 0                      | 0                         | implement a transpiration calculation based on the Penman-Monteith Equation                                     |
| GROWTH\_RESP 0                                 | 0                         | explicitly model growth resp., rather than including with maint. resp.                                          |
| LLOYD\_TAYLOR 0                                | 0                         | use Lloyd-Taylor model for soil respiration, in which temperature sensitivity decreases at higher temperatures? |
| SEASONAL\_R\_SOIL 0 && !LLOYD\_TAYLOR          | 0                         | use different parameters for soil resp. (baseSoilResp and soilRespQ10) when tsoil < (some threshold)?           |
| WATER\_PSN 1                                   | 1                         | does soil moisture affect photosynthesis?                                                                       |
| WATER\_HRESP 1                                 | 1                         | does soil moisture affect heterotrophic respiration?                                                            |
| DAYCENT\_WATER\_HRESP 0 && WATER\_HRESP        | 0                         | use DAYCENT soil moisture function?                                                                             |
| MODEL\_WATER 1                                 | 1                         | do we model soil water (and ignore soilWetness)?                                                                |
| COMPLEX\_WATER 1 && MODEL\_WATER               | 1                         | do we use a more complex water submodel? (model evaporation as well as transpiration)                           |
| LITTER\_WATER 0 && (COMPLEX\_WATER)            | 0                         | do we have a separate litter water layer, used for evaporation?                                                 |
| LITTER\_WATER\_DRAINAGE 1 && (LITTER\_WATER)   | 0                         | does water from the top layer drain down into bottom layer even if top layer not overflowing?                   |
| SNOW (1 \|\| (COMPLEX\_WATER)) && MODEL\_WATER | 1                         | keep track of snowpack, rather than assuming all precip. is liquid                                              |
| GDD 0                                          | 0                         | use GDD to determine leaf growth? (note: mutually exclusive with SOIL\_PHENOL)                                  |
| SOIL\_PHENOL 0 && !GDD                         | 0                         | use soil temp. to determine leaf growth? (note: mutually exclusive with GDD)                                    |
| LITTER\_POOL 0                                 | 0                         | have extra litter pool, in addition to soil c pool                                                              |
| SOIL\_MULTIPOOL 0 && !LITTER\_POOL             | 0                         | do we have a multipool approach to model soils?                                                                 |
| NUMBER\_SOIL\_CARBON\_POOLS 3                  | 3                         | number of pools we want to have. Equal to 1 if SOIL\_MULTIPOOL is 0                                             |
| SOIL\_QUALITY 0 && SOIL\_MULTIPOOL             | 0                         | do we have a soil quality submodel?                                                                             |
| MICROBES 0 && !SOIL\_MULTIPOOL                 | 0                         | do we utilize microbes. This will only be an option if SOIL\_MULTIPOOL is 0 and MICROBES is 1                   |
| STOICHIOMETRY 0 && MICROBES                    | 0                         | do we utilize stoichometric considerations for the microbial pool?                                              |
| ROOTS 0                                        | 0                         | do we model root dynamics? If no, roots are part of wood pool. If yes, split into coarse and fine roots|
| MODIS 0                                        | 0                         | do we use modis FPAR data to constrain GPP?                                                                     |
| C\_WEIGHT 12.0                                 | 12                        | molecular weight of carbon                                                                                      |
| MEAN\_NPP\_DAYS 5                              | 5                         | over how many days do we keep the running mean                                                                  |
| MEAN\_NPP\_MAX\_ENTRIES                        | MEAN\_NPP\_DAYS\*50       | assume that the most pts we can have is two per hour                                                            |
| MEAN\_GPP\_SOIL\_DAYS 5                        | 5                         | over how many days do we keep the running mean                                                                  |
| MEAN\_GPP\_SOIL\_MAX\_ENTRIES                  | MEAN\_GPP\_SOIL\_DAYS\*50 | assume that the most pts we can have is one per hour                                                            |
| LAMBDA                                         | 2501000                   | latent heat of vaporization (J/kg)                                                                              |
| LAMBDA\_S                                      | 2835000                   | latent heat of sublimation (J/kg)                                                                               |
| RHO                                            | 1.3                       | air density (kg/m^3)                                                                                            |
| CP                                             | 1005.                     | specific heat of air (J/(kg K))                                                                                 |
| GAMMA                                          | 66                        | psychometric constant (Pa/K)                                                                                    |
| E\_STAR\_SNOW                                  | 0.6                       | approximate saturation vapor pressure at 0°C (kPa)                                                              |
|                                                |                           |                                                                                                                 |

## Drivers

### Climate

For each step of the model, for each location, the following inputs are needed. These are provided in a file named `<sitename>.clim` with the following columns:

| col | parameter | description | units | notes |
| -- | ----------- | --------------------------------------------------- | ---------------------------------------- | ----------------------------------------------------------------------------------------------------------- |
| 1  | loc         | spatial location index          |               | maps to param-spatial file, can be 0 for a single site, as when used by PEcAn |
| 2  | year        | year of start of this timestep  |               | integer, e.g. 2010|
| 3  | day         | day of start of this timestep   |               | integer where 1 = Jan 1|
| 4  | time        | time of start of this timestep  | hours after midnight | e.g. noon = 12.0, midnight = 0.0, can be a fraction |
| 5  | length      | length of this timestep         | days          | variable-length timesteps allowed, typically not used |
| 6  | tair        | avg. air temp for this time step| degrees Celsius |     |
| 7  | tsoil       | average soil temperature for this time step  | degrees Celsius| can be estimated from Tair  |
| 8  | par         | average photosynthetically active radiation (PAR) for this time step  | $\text{Einsteins} \cdot m^{-2} \text{ground area} \cdot \text{time step}^{-1}$  | input is in Einsteins \* m^-2 ground area, summed over entire time step |
| 9  | precip      | total precip. for this time step| cm            | input is in mm; water equivilant - either rain or snow   |
| 10 | vpd         | average vapor pressure deficit   | kPa          | input is in Pa, can be calculated from air temperature and relative humidity.|
| 11 | vpdSoil     | average vapor pressure deficit between soil and air | kPa | input is in Pa ; differs from vpd in that saturation vapor pressure is calculated using Tsoil rather than Tair |
| 12 | vPress      | average vapor pressure in canopy airspace | kPa | input is in Pa |
| 13 | wspd        | avg. wind speed                   | m/s         |                |
| 14 | soilWetness | fractional soil wetness          | unitless (0-1) | $f_\text{WHC}$; Used if `MODEL_WATER=0`; if `MODEL_WATER=1`, soil wetness is simulated|

#### Examples of climate files:

Column names are not used, but are:

```
loc	year day  time length tair tsoil par    precip vpd   vpdSoil vPress wspd   soilWetness
```

**Half-hour time step**

```
0	1998 305  0.00    -1800   1.9000   1.2719   0.0000   0.0000 109.5364  77.5454 726.6196   1.6300   0.0000
0	1998 305  0.50    -1800   1.9000   1.1832   0.0000   0.0000 109.5364  73.1254 726.6196   1.6300   0.0000
0	1998 305  1.00    -1800   2.0300   1.1171   0.0000   0.0000 110.4243  63.9567 732.5092   0.6800   0.0000
0	1998 305  1.50    -1800   2.0300   1.0439   0.0000   0.0000 110.4243  60.3450 732.5092   0.6800   0.0000
```

**Variable time step**

```
0	  1998 305  0.00  0.292 1.5  0.8   0.0000 0.0000 105.8 70.1    711.6  0.9200 0.0000
0	  1998 305  7.00  0.417 3.6  1.8   5.6016 0.0000 125.7 23.5    809.4  1.1270 0.0000
0	  1998 305 17.00  0.583 1.9  1.3   0.0000 0.0000 108.1 75.9    732.7  1.1350 0.0000
0	  1998 306  7.00  0.417 2.2  1.4   2.7104 1.0000 114.1 71.6    741.8  0.9690 0.0000
```


### Agronomic Events

For managed ecosystems, the following inputs are provided in a file named `events.in` with the following columns:

| col   | parameter   | description                        | units       | notes                                 |
|-------|-------------|------------------------------------|-------------|---------------------------------------|
| 1     | loc         | spatial location index             |             | maps to param-spatial file            |
| 2     | year        | year of start of this timestep     |             | e.g. 2010                             |
| 3     | day         | day of start of this timestep      | Day of year | 1 = Jan 1                             |
| 4     | event_type  | type of event                      |             | one of plant, harv, till, fert, irrig |
| 5...n | event_param | parameter associated with event    |             | see table below                       |

- Agronomic events are stored in events.in, one event per row
- Events in the file are sorted first by location, and then chronologically
- Events are specified by year and day (no hourly timestamp)
- It is assumed that there is one (or more) records in the climate file for each location/day that appears in the events file
  - We will throw an error if we find an event with no corresponding climate record
- Events are processed with the first climate record that occurs for the relevant location/day as an instantaneous one-time change
  - We may need events with duration later, spec TBD. Tillage is likely in this bucket.
- The effects of an event are applied after fluxes are calculated for the current climate record; they are applied as a delta to one or more state variables, as required


| parameter   | col | req? |  description                                 |
|-------------|:---:|:----:|----------------------------------------------|
| amount      |  5  |  Y   |  Amount added (cm/d)                         |
| method      |  6  |  Y   |  0=canopy<br>1=soil<br>2=flood (placeholder) |

Model representation: an irrigation event increases soil moisture. Canopy irrigation also loses some moisture to evaporation.

Specifically: 
- amount is listed as cm/d, but as events are specified per-day, this is treated as `cm` of water added on that day
- For method=soil, this amount of water is added directly to the `soilWater` state variable 
- For method=canopy, a fraction of the irrigation water (determined by input param `immedEvapFrac`) is added to the flux state variable `immedEvap`, with the remainder going to `soilWater`.
- Initial implementation assumes that LITTER_WATER is not on. This might be revisited at a later date.

Notes:
- irrigation could also directly change the soil moisture content rather than adding water as a flux. This could be used to represent an irrigation program that sets a moisture range and turns irrigation on at the low end and off at the high end of the range.

#### Fertilization Events

| parameter | col | req? | description                                                     |
|-----------|:---:|:----:|-----------------------------------------------------------------|
| org-N     |  5  |  Y   | g N / m2                                                        |
| org-C     |  6  |  Y   | g C / m2                                                        |
| min-N     |  7  |  Y   | g N / m2 (NH4+NO3 in one pool model; NH4 in two pool model)     |
| min-N2    |  8  |  Y*  | g N / m2 (*not unused in one pool model, NO3 in two pool model) |

  - model representation: increases size of mineral N and litter C and N. Urea-N is assumed to be mineral N or NH4.
  - notes: PEcAn will handle conversion from fertilizer amount and type to mass of N and C allocated to different pools 

#### Tillage Events

| parameter                     | col | req? | description              |
|-------------------------------|:---:|:----:|--------------------------|
| SOM decomposition modifier    |  5  |  Y   | % increase in $K_{dec}$  |
| litter decomposition modifier |  6  |  Y   | % increase in $K_{lit}$  |

  - model representation:
    - increase k for one month, amount proportional to depth
    - transfer litter C and N to soil pool
  - notes: could also alter bulk density and other soil properties

#### Planting Events

| parameter     | col | req? | description                                  |
|---------------|:---:|:----:|----------------------------------------------|
| leaf-C        |  5  |  Y   | C added to leaf pool (g C / m2)              |
| wood-C        |  6  |  Y   | C added to above-ground wood pool (g C / m2) |
| fine-root-C   |  7  |  Y   | C added to fine root pool (g C / m2)         |
| coarse-root-C |  8  |  Y   | C added to coarse root pool (g C / m2)       |

- model representation: 
  - Date of event is the date of emergence, not the date of actual planting 
  - Increases size of carbon pools by the amount of each respective parameter
  - $N$ pools are calculated from $CN$ stoichiometric ratios.
- notes: PFT (crop type) is not an input parameter for a planting event because SIPNET only represents a single PFT.

#### Harvest Events

| parameter                                                  | col | req? | description           |
|------------------------------------------------------------|:---:|:----:|-----------------------|
| fraction of aboveground biomass removed                    |  5  |  Y   |                       |
| fraction of belowground biomass removed                    |  6  |  N   | default = 0           |
| fraction of aboveground biomass transferred to litter pool |  7  |  N   | default = 1 - removed |
| fraction of belowground biomass transferred to litter pool |  8  |  N   | default = 1 - removed |

- model representation:
  - biomass C and N pools are either removed or added to litter
   - for annuals or plants terminated, no biomass remains (col 5 + col 7 = 1 and col 6 + col 8 = 1). 
  - for perennials, some biomass may remain (col 5 + col 7 <= 1 and col 6 + col 8 <= 1; remainder is living).
   - root biomass is only removed for root crops
 
#### Example of events file:

```
1  2022  40  irrig  5   1        # 5cm canopy irrigation on day 40
1  2022  40  fert   10 5 0 0     # fertilized with 10 g / m2 NO3-N and 5 g / m2 NH4-N on day 40
1  2022  35  till   0.2 0.3      # tilled on day 35, soil organic matter pool decomposition rate increases by 20% and soil litter pool decomposition rate increases by 30% 
1  2022  50  plant  10 3 2 5     # plant emergence on day 50 with 10/3/2/4 g C / m2, respectively, added to the leaf/wood/fine root/coarse root pools 
1  2022  250 harv   0.1          # harvest 10% of aboveground plant biomass on day 250
```

## Outputs

|    | Symbol      | Parameter Name     | Definition                     | Units       |
| -- | ----------- | ------------------ | ------------------------------ | ----------- |
| 1  |             |loc                 | spatial location index         |             |
| 2  |             |year                | year of start of this timestep |             |
| 3  |             |day                 | day of start of this timestep  |             |
| 4  |             |time                | time of start of this timestep |             |
| 5  |             |plantWoodC          | carbon in wood                 | g C/m$^2$   |
| 6  |             |plantLeafC          | carbon in leaves               | g C/m$^2$   |
| 7  |             |soil                | carbon in mineral soil         | g C/m$^2$   |
| 8  |             |microbeC            | carbon in soil microbes        | g C/m$^2$   |
| 9  |             |coarseRootC         | carbon in coarse roots         | g C/m$^2$   |
| 10 |             |fineRootC           | carbon in fine roots           | g C/m$^2$   |
| 11 |             |litter              | carbon in litter               | g C/m$^2$   |
| 12 |             |litterWater         | moisture in litter layer       | cm          |
| 13 |             |soilWater           | moisture in soil               | cm          |
| 14 |$f_\text{WHC}$|soilWetnessFrac    | moisture in soil as fraction   |             |
| 15 |             |snow                | snow water                     | cm          |
| 16 |             |npp                 | net primary production         | g C/m$^2$   |
| 17 |             |nee                 | net ecosystem production       | g C/m$^2$   |
| 18 |             |cumNEE              | cumulative nee                 | g C/m$^2$   |
| 19 |  $GPP$      |gpp                 | gross ecosystem production     | g C/m$^2$   |
| 20 |  $R_{A,\text{above}}$ |rAboveground  | plant respiration above ground | g C/m$^2$   |
| 21 |  $R_H$      |rSoil               | soil respiration               | g C/m$^2$   |
| 22 |  $R_{A\text{, root}}$  |rRoot               | root respiration     | g C/m$^2$   |
| 23 |  $R$        |rtot                | total respiration              | g C/m$^2$   |
| 24 |             |fluxestranspiration | transpiration                  | cm          |
|    | $F^N_\text{vol}$ |fluxesn2o      | Nitrous Oxide flux             | g N/m$^2$ / timestep |
|    | $F^C_{\text{CH}_4}$  |fluxesch4    | Methane Flux                   | g C/m$^2$ / timestep |
|    | $F^N_\text{vol}$ |fluxesn2o      | Nitrous Oxide flux             | g N/m$^2$ / timestep |
|    | $F^C_{\text{CH}_4}$  |fluxesch4    | Methane Flux                   | g C/m$^2$ / timestep |

<!--

|    | $F^C_\text{CH_4}$  |fluxesch4    | Methane Flux                   | g C/m$^2$ / timestep |
# unused content

TODO: update and move the following terms used in model description to appropriate tables

| Symbol          | Description  |
|-----------------|-------------|
| $F_{T,Q10}$ | Temperature dependence using Q10 relationship  |  
| $F_{T,opt}$ | Temperature dependence based on optimal temperature |
| $W_{\text{soil}}$ | Soil water content                                                   |
| $W_{\text{WHC}}$ | Soil Water Holding Capacity                                           |
| $f_{\text{anaer}}$ | Fraction of anaerobic soil                                           |
| $R_{\text{mineralization,N}}$ | Rate of nitrogen mineralization                                 |
| $R_{\text{leach,}N_{\text{mineral}}}$ | Rate of mineral nitrogen leaching                |
| $R_{\text{vol,}N_{\text{mineral}}}$ | Rate of mineral nitrogen volatilization                  |
| $R_{\text{fert,}N_{\text{mineral}}}$ | Rate of mineral nitrogen fertilization                   |
| $R_{\text{fert,}N_{\text{org}}}$ | Rate of organic nitrogen fertilization input                    |
| $R_{\text{meth}}$ | Rate of methane production                                           |
| $R_{\text{methox}}$ | Rate of methane oxidation                                          |
-->
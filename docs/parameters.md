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

# SIPNET Model States and Parameters {#sec-parameters}

Lists SIPNET state variables and tunable parameters, mapping symbols to the
model equations, configuration names, units, and I/O fields. See
[Model Inputs](user-guide/model-inputs.md) and
[Model Outputs](user-guide/model-outputs.md) for file formats. Unless noted,
pools are mass per ground area and rates are mass per area per day. The actual parameter set that is used depends on the configured model structure. For equation references, see the [model structure](model-structure.md) documentation.

## Notation {#sec-notation}

### Variables (Pools, Fluxes, and Parameters)


| **Category**              | **Symbol** | **Description**                                        |
| :------------------------ | :--------- | :----------------------------------------------------- |
| **State variables**       |            |                                                        |
|                           | $C$        | Carbon pool                                            |
|                           | $N$        | Nitrogen pool                                          |
|                           | $W$        | Water pool or content                                  |
|                           | $CN$       | Carbon-to-Nitrogen ratio                               |
| **Fluxes and rates**      |            |                                                        |
|                           | $F$        | Generic flux of carbon, nitrogen, or water             |
|                           | $A$        | Photosynthetic assimilation (net photosynthesis)       |
|                           | $R$        | Respiration flux                                       |
|                           | $ET$       | Evapotranspiration                                     |
|                           | $GPP$      | Gross Primary Production                               |
|                           | $NPP$      | Net Primary Production                                 |
|                           | $NEE$      | Net Ecosystem Exchange                                 |
| **Environmental drivers** |            |                                                        |
|                           | $T$        | Temperature                                            |
|                           | $VPD$      | Vapor Pressure Deficit                                 |
|                           | $PAR$      | Photosynthetically Active Radiation                    |
|                           | $LAI$      | Leaf Area Index                                        |
| **Parameters**            |            |                                                        |
|                           | $K$        | Rate constant (e.g., for decomposition or respiration) |
|                           | $Q_{10}$   | Temperature sensitivity coefficient                    |
|                           | $\alpha$   | Fraction of NPP allocated to a plant pool              |
|                           | $f$        | Fraction of a pool or flux other than NPP              |
|                           | $k$        | Scaling factor                                         |
|                           | $D$        | Dependency or damping function                         |

### Subscripts (Temporal, Spatial, or Contextual Identifiers)


| **Category**                             | **Subscript**          | **Description**                                       |
| :--------------------------------------- | :--------------------- | :---------------------------------------------------- |
| **Temporal identifiers**                 |                        |                                                       |
|                                          | $X_0$                  | Initial value                                         |
|                                          | $X_t$                  | Value at time $t$                                     |
|                                          | $X_d$                  | Daily value or average                                |
|                                          | $X_\text{avg}$         | Average value (e.g., over a timestep or spatial area) |
|                                          | $X_\text{max}$         | Maximum value (e.g., temperature or rate)             |
|                                          | $X_\text{min}$         | Minimum value (e.g., temperature or rate)             |
|                                          | $X_\text{opt}$         | Optimal value (e.g., temperature or rate)             |
| **Structural components**                |                        |                                                       |
|                                          | $X_\text{leaf}$        | Leaf pools or fluxes                                  |
|                                          | $X_\text{wood}$        | Wood pools or fluxes                                  |
|                                          | $X_\text{root}$        | Root pool                                             |
|                                          | $X_\text{fine root}$   | Fine root pool                                        |
|                                          | $X_\text{coarse root}$ | Coarse root pool                                      |
|                                          | $X_\text{soil}$        | Soil pools or processes                               |
|                                          | $X_\text{litter}$      | Litter pools or processes                             |
|                                          | $X_\text{veg}$         | Vegetation processes (general)                        |
| **Processes context**                    |                        |                                                       |
|                                          | $X_\text{resp}$        | Respiration processes                                 |
|                                          | $X_\text{dec}$         | Decomposition processes                               |
|                                          | $X_\text{vol}$         | Volatilization processes                              |
| **Chemical / environmental identifiers** |                        |                                                       |
|                                          | $X_\text{org}$         | Organic forms                                         |
|                                          | $X_\text{mineral}$     | Mineral forms                                         |
|                                          | $X_{\text{anaer}}$     | Anaerobic soil conditions                             |

Subscripts may be used in combination, e.g. $X_{\text{soil,mineral},0}$.

## Run-time Parameters

Run-time parameters can change from one run to the next, or when the model is stopped and restarted. These include initial state values and parameters related to plant physiology, soil physics, and biogeochemical cycling.

### Initial state values

|     | Symbol                     | Parameter Name  | Definition                                                               | Units                                                | notes                                                                              |
| --- | -------------------------- | --------------- | ------------------------------------------------------------------------ | ---------------------------------------------------- | ---------------------------------------------------------------------------------- |
| 1   | $C_{\text{wood},0}$        | plantWoodInit   | Initial wood carbon                                                      | $\text{g C} \cdot \text{m}^{-2} \text{ ground area}$ | above-ground + roots                                                               |
| 2   | $LAI_0$                    | laiInit         | Initial leaf area                                                        | m^2 leaves \* m^-2 ground area                       | multiply by SLW to get initial plant leaf C: $C_{\text{leaf},0} = LAI_0 \cdot SLW$ |
| 3   | $C_{\text{litter},0}$      | litterInit      | Initial litter carbon                                                    | $\text{g C} \cdot \text{m}^{-2} \text{ ground area}$ |                                                                                    |
| 4   | $C_{\text{soil},0}$        | soilInit        | Initial soil carbon                                                      | $\text{g C} \cdot \text{m}^{-2} \text{ ground area}$ |                                                                                    |
| 5   | $W_{\text{litter},0}$      | litterWFracInit |                                                                          | unitless                                             | fraction of litterWHC                                                              |
| 6   | $W_{\text{soil},0}$        | soilWFracInit   |                                                                          | unitless                                             | fraction of soilWHC                                                                |
|     | $N_{\text{org, litter},0}$ |                 | Initial litter organic nitrogen content                                  | g N m$^{-2}$                                         |                                                                                    |
|     | $N_{\text{org, soil},0}$   |                 | Initial soil organic nitrogen content                                    | g N m$^{-2}$                                         |                                                                                    |
|     | $N_{\text{min, soil},0}$   |                 | Initial soil mineral nitrogen content                                    | g N m$^{-2}$                                         |                                                                                    |
|     | ${CH_4}_{\text{soil},0}$   |                 | Initial methane concentration in the soil                                | g C m$^{-2}$                                         |                                                                                    |
|     | ${N_2O}_{\text{soil},0}$   |                 | Nitrous oxide concentration in the soil                                  | g N m$^{-2}$                                         |                                                                                    |
|     | $f_{\text{fine root},0}$   | fineRootFrac    | Fraction of `plantWoodInit` allocated to initial fine root carbon pool   |                                                      |                                                                                    |
|     | $f_{\text{coarse root},0}$ | coarseRootFrac  | Fraction of `plantWoodInit` allocated to initial coarse root carbon pool |                                                      |                                                                                    |

<!--not used in CCMMF

| 7 |                                          | snowInit        | Initial snow water                        | cm water equiv.                |                                                   |
|   |                                          | microbeInit     |                                           |                                |                                                   |



<!--if separating N_min into NH4 and NO3

### Initial state values 

|     | Symbol                   | Parameter Name | Definition                    | Units        | notes |
| --- | ------------------------ | -------------- | ----------------------------- | ------------ | ----- |
|     | ${NH_4}_{\text{soil},0}$ |                | Initial soil ammonium content | g N m$^{-2}$ |       |
|     | ${NO_3}_{\text{soil},0}$ |                | Initial soil nitrate content  | g N m$^{-2}$ |       |

 

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

|     | Symbol               | Parameter Name   | Definition                                                              | Units                                                   | notes                                          |
| --- | -------------------- | ---------------- | ----------------------------------------------------------------------- | ------------------------------------------------------- | ---------------------------------------------- |
| 17  | $D_{\text{on}}$      | leafOnDay        | Day of year when leaves appear                                          | day of year                                             |                                                |
| 18  |                      | gddLeafOn        | with gdd-based phenology, gdd threshold for leaf appearance             |                                                         |                                                |
| 19  |                      | soilTempLeafOn   | with soil temp-based phenology, soil temp threshold for leaf appearance |                                                         |                                                |
| 20  | $D_{\text{off}}$     | leafOffDay       | Day of year for leaf drop                                               |                                                         |                                                |
| 21  |                      | leafGrowth       | additional leaf growth at start of growing season                       | $\text{g C} \cdot \text{m}^{-2} \text{ ground}$         |                                                |
| 22  |                      | fracLeafFall     | additional fraction of leaves that fall at end of growing season        |                                                         |                                                |
| 23  | $\alpha_\text{leaf}$ | leafAllocation   | fraction of NPP allocated to leaf growth                                |                                                         |                                                |
| 24  | $K_{leaf}$           | leafTurnoverRate | average turnover rate of leaves                                         | $\text{y}^{-1}$                                         | converted to per-day rate internally           |
|     | $L_{\text{max}}$     |                  | Maximum leaf area index obtained                                        | $\text{m}^2 \text{ leaf } \text{m}^{-2} \text{ ground}$ | ? from Braswell et al 2005; can't find in code |


### Allocation parameters

|     | Symbol                    | Parameter Name      | Definition                                                      | Units    | notes              |
| --- | ------------------------- | ------------------- | --------------------------------------------------------------- | -------- | ------------------ |
| 64  |                           | fineRootFrac        | fraction of wood carbon allocated to fine root                  |          |                    |
| 65  |                           | coarseRootFrac      | fraction of wood carbon that is coarse root                     |          |                    |
| 66  | $\alpha_\text{fine root}$ | fineRootAllocation  | fraction of NPP allocated to fine roots                         |          |                    |
| 67  | $\alpha_\text{wood}$      | woodAllocation      | fraction of NPP allocated to wood                               |          |                    |
| 68  |                           | fineRootExudation   | fraction of GPP from fine roots exuded to the soil[^exudates]   | fraction | Pulsing parameters |
| 69  |                           | coarseRootExudation | fraction of GPP from coarse roots exuded to the soil[^exudates] | fraction | Pulsing parameters |

[^exudates]: Fine and coarse root exudation are calculated as a fraction of GPP, but the exudates are subtracted from the fine and coarse root pools, respectively. <!--Note that previous versions incorrectly defined fine root exudates as a fraction of NPP-->

### Autotrophic respiration parameters

|     | Symbol                | Parameter Name      | Definition                                                               | Units                                          | notes                                                                                                                                              |
| --- | --------------------- | ------------------- | ------------------------------------------------------------------------ | ---------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------- |
| 25  | $R_{\text{a,wood},0}$ | baseVegResp         | Wood maintenance respiration rate at $0^\circ C$                         | g C respired \* g$^{-1}$ plant C \* day$^{-1}$ | read in as per-year rate only counts plant wood C; leaves handled elsewhere (both above and below-ground: assumed for now to have same resp. rate) |
| 26  | $Q_{10v}$             | vegRespQ10          | Vegetation respiration Q10                                               |                                                | Scalar determining effect of temp on veg. resp.                                                                                                    |
| 27  |                       | growthRespFrac      | growth respiration as a fraction of recent mean NPP.                     |                                                |
| 28  |                       | frozenSoilFolREff   | amount that foliar resp. is shutdown if soil is frozen                   |                                                | 0 = full shutdown, 1 = no shutdown                                                                                                                 |
| 29  |                       | frozenSoilThreshold | soil temperature below which frozenSoilFolREff and frozenSoilEff kick in | °C                                             |                                                                                                                                                    |  |  |
| 72  |                       | baseFineRootResp    | base respiration rate of fine roots                                      | $\text{y}^{-1}$                                | per year rate                                                                                                                                      |
| 73  |                       | baseCoarseRootResp  | base respiration rate of coarse roots                                    | $\text{y}^{-1}$                                | per year rate                                                                                                                                      |


### Soil respiration parameters

|     | Symbol              | Parameter Name      | Definition                                                                          | Units                                         | notes                                                 |
| --- | ------------------- | ------------------- | ----------------------------------------------------------------------------------- | --------------------------------------------- | ----------------------------------------------------- |
| 30  | $K_\text{litter}$   | litterBreakdownRate | rate at which litter is converted to soil / respired at 0°C and max soil moisture   | g C broken down \* g^-1 litter C \* day^-1    | read in as per-year rate                              |
| 31  |                     | fracLitterRespired  | of the litter broken down, fraction respired (the rest is transferred to soil pool) |                                               |                                                       |
| 32  | $K_{dec}$           | baseSoilResp        | Soil respiration rate at $0 ^{\circ}\text{C}$ and moisture saturated soil           | g C respired \* g$^{-1}$ soil C \* day$^{-1}$ | read in as per-year rate                              |
| 34  | $Q_{10s}$           | soilRespQ10         | Soil respiration Q10                                                                |                                               | scalar determining effect of temp on soil respiration |
| 39  |                     | soilRespMoistEffect | scalar determining effect of moisture on soil resp.                                 |                                               |                                                       |
|     |                     | baseMicrobeResp     |                                                                                     |                                               |                                                       |
| new | $f_{\textrm{till}}$ | tillageEff          | Effect of tillage on decomposition that exponentially decays over time              | fraction                                      | Per‑event in `events.in`; 0 = no effect               |

- $R_{dec}$: Rate of decomposition $(\text{day}^{-1})$ 
- $Q_{10dec}$: Temperature coefficient for $R_{dec}$ (unitless)

### Nitrogen Cycle Parameters

|     | Symbol               | Parameter Name      | Definition                                                                                   | Units        | notes                      |
| --- | -------------------- | ------------------- | -------------------------------------------------------------------------------------------- | ------------ | -------------------------- |
| new | $N_{\text{min},0}$   | mineralNInit        | Initial soil mineral nitrogen pool                                                           | g N m$^{-2}$ | Initializes $N_\text{min}$ |
| new | $K_\text{vol}$       | nVolatilizationFrac | Fraction of $N_\text{min}$ volatilized per day (modulated by temperature and moisture)       | day$^{-1}$   | Eq. (17)                   |
| new | $f^N_{\text{leach}}$ | nLeachingFrac       | Leaching coefficient applied to $N_\text{min}$ scaled by drainage                            | day$^{-1}$   | Eq. (18)                   |
| new | $f_{\text{fix,max}}$ | nFixFracMax         | Maximum fraction of plant N demand that can be met by biological N fixation under low soil N | fraction     | Eq. (19)                   |
| new | $K_N$                | nFixHalfSatMinN     | Mineral N level at which fixation suppression factor $D_{N_\text{min}}$ equals 0.5           | g N m$^{-2}$ | Eq. (19a)                  |

### Methane parameters

- $R_{meth}$: Rate of methane production $(\text{day}^{-1})$
- $K_{meth}$: Rate constant for methane production under anaerobic conditions $(\text{day}^{-1})$
- $K_{methox}$: Rate constant, methane oxidation $(\text{day}^{-1})$

### Moisture-related parameters

|     | Symbol                   | Parameter Name  | Definition                                                                                                             | Units                                           | notes                                                                                                                                          |
| --- | ------------------------ | --------------- | ---------------------------------------------------------------------------------------------------------------------- | ----------------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------- |
| 40  | $f_{\text{trans,avail}}$ | waterRemoveFrac | fraction of plant available soil water which can be removed in one day by transpiration without water stress occurring |                                                 |                                                                                                                                                |
| new | $f_\text{drain,0}$       | waterDrainFrac  | fraction of plant available soil water which can be removed in one day by drainage                                     | $d^{-1}$                                        | default 1 for well drained soils                                                                                                               |
| 41  |                          | frozenSoilEff   | fraction of water that is available if soil is frozen (0 = none available, 1 = all still avail.)                       |                                                 | if frozenSoilEff = 0, then shut down psn. even if WATER\_PSN = 0, if soil is frozen (if frozenSoilEff > 0, it has no effect if WATER\_PSN = 0) |
| 42  |                          | wueConst        | water use efficiency constant                                                                                          |                                                 |                                                                                                                                                |
| 43  |                          | litterWHC       | litter (evaporative layer) water holding capacity                                                                      | cm                                              |                                                                                                                                                |
| 44  |                          | soilWHC         | soil (transpiration layer) water holding capacity                                                                      | cm                                              |                                                                                                                                                |
| 45  | $f_\text{intercept}      | immedEvapFrac   | fraction of rain that is immediately intercepted & evaporated                                                          |                                                 |                                                                                                                                                |
| 46  |                          | fastFlowFrac    | fraction of water entering soil that goes directly to drainage                                                         |                                                 |                                                                                                                                                |
|     | $k_\text{SOM,drain}$     |
| 47  |                          | snowMelt        | rate at which snow melts                                                                                               | cm water equivavlent per degree Celsius per day |                                                                                                                                                |
| 49  |                          | rdConst         | scalar determining amount of aerodynamic resistance                                                                    |                                                 |                                                                                                                                                |
| 50  |                          | rSoilConst1     |                                                                                                                        |                                                 | soil resistance = e^(rSoilConst1 - rSoilConst2 \* W1) , where W1 = (litterWater/litterWHC)                                                     |
| 51  |                          | rSoilConst2     |                                                                                                                        |                                                 | soil resistance = e^(rSoilConst1 - rSoilConst2 \* W1) , where W1 = (litterWater/litterWHC)                                                     |



### Tree physiological parameters

|     | Symbol                 | Parameter Name         | Definition                             | Units                | notes                                                              |
| --- | ---------------------- | ---------------------- | -------------------------------------- | -------------------- | ------------------------------------------------------------------ |
| 53  | $SLW$                  | leafCSpWt              |                                        | g C * m^-2 leaf area |                                                                    |
| 54  | $C_{frac}$             | cFracLeaf              |                                        | g leaf C * g^-1 leaf |                                                                    |
| 55  | $K_\text{wood}$        | woodTurnoverRate       | average turnover rate of woody plant C | $\text{y}^{-1}$      | converted to per-day rate internally; leaf loss handled separately |
| 70  | $K_\text{fine root}$   | fineRootTurnoverRate   | turnover of fine roots                 | $\text{y}^{-1}$      | converted to per-day rate internally                               |
| 71  | $K_\text{coarse root}$ | coarseRootTurnoverRate | turnover of coarse roots               | $\text{y}^{-1}$      | converted to per-day rate internally                               |



<!--
### Quality model parameters


|     | Symbol | Parameter Name   | Definition                                | Units                 | notes                         |
| --- | ------ | ---------------- | ----------------------------------------- | --------------------- | ----------------------------- |
| 58  |        | efficiency       | conversion efficiency of ingested carbon  |                       | Microbe & Stoichiometry model |
| 59  |        | maxIngestionRate | maximum ingestion rate of the microbe     | hr-1                  | Microbe & Stoichiometry model |
| 60  |        | halfSatIngestion | half saturation ingestion rate of microbe | mg C g-1 soil         | Microbe & Stoichiometry model |
| 63  |        | microbeInit      |                                           | mg C / g soil microbe | initial carbon amount         | --> |



<!-- Not used in CCMMF (two Q10s , soil and veg); no microbes
| 74 |                   | fineRootQ10            | Q10 of fine roots                                  |                       |                               |
| 75 |                   | coarseRootQ10          | Q10 of coarse roots                                |                       |                               |
| 76 |                   | baseMicrobeResp        | base respiration rate of microbes                  |                       |                               |
| 77 |                   | microbeQ10             | Q10 of microbes                                |                       |                               |
| 78 |                   | microbePulseEff        | fraction of exudates that microbes immediately use |                       | Pulsing parameters            |

-->

## Hard-coded Values

| Parameter                   | Value                   | Description                                          |
| --------------------------- | ----------------------- | ---------------------------------------------------- |
| `C_WEIGHT`                  | 12.0                    | molecular weight of carbon                           |
| `MEAN_NPP_DAYS`             | 5                       | over how many days do we keep the running mean       |
| `MEAN_NPP_MAX_ENTRIES`      | `MEAN_NPP_DAYS`*50      | assume that the most pts we can have is two per hour |
| `MEAN_GPP_SOIL_DAYS`        | 5                       | over how many days do we keep the running mean       |
| `MEAN_GPP_SOIL_MAX_ENTRIES` | `MEAN_GPP_SOIL_DAYS`*50 | assume that the most pts we can have is one per hour |
| `LAMBDA`                    | 2501000                 | latent heat of vaporization (J/kg)                   |
| `LAMBDA_S`                  | 2835000                 | latent heat of sublimation (J/kg)                    |
| `RHO`                       | 1.3                     | air density (kg/m^3)                                 |
| `CP`                        | 1005.                   | specific heat of air (J/(kg K))                      |
| `GAMMA`                     | 66                      | psychometric constant (Pa/K)                         |
| `E_STAR_SNOW`               | 0.6                     | approximate saturation vapor pressure at 0°C (kPa)   |


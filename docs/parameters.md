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

## Notation

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
|                           | $R_H$      | Heterotrophic respiration (soil/litter decomposition) |
|                           | $R_A$      | Autotrophic respiration (vegetation respiration)       |
|                           | $ET$       | Evapotranspiration                                     |
|                           | $T$        | Transpiration                                          |
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
|                           | $SLW$      | Specific Leaf Weight (leaf carbon per unit leaf area)  |

**Note on superscripts:** Most variables use superscripts for process context (e.g., $X^{\text{growth}}$ for growth respiration). However, certain fluxes follow established scientific conventions and are written without superscripts: carbon fluxes ($NPP$, $GPP$, $Rh$, $Ra$) and water fluxes ($ET$, $T$). These terms are widely used in the scientific literature as standard measurements and retain this notation even when combined with subscripts (e.g., $Rh_{\text{litter}}$).

### Subscripts (Temporal, Spatial, or Contextual Identifiers)

| **Category**                             | **Subscript**          | **Description**                                       |
| :--------------------------------------- | :--------------------- | :---------------------------------------------------- |
| **Temporal identifiers**                 |                        |                                                       |
|                                          | $X_0$                  | Initial value                                         |
|                                          | $X_t$                  | Value at time $t$                                     |
|                                          | $X_d$                  | Daily value or average                                |
|                                          | $X_{\text{avg}}$       | Average value (e.g., over a timestep or spatial area) |
|                                          | $X_{\text{max}}$       | Maximum value (e.g., temperature or rate)             |
|                                          | $X_{\text{min}}$       | Minimum value (e.g., temperature or rate)             |
|                                          | $X_{\text{opt}}$       | Optimal value (e.g., temperature or rate)             |
| **Structural components**                |                        |                                                       |
|                                          | $X_{\text{leaf}}$      | Leaf pools or fluxes                                  |
|                                          | $X_{\text{wood}}$      | Wood pools or fluxes                                  |
|                                          | $X_{\text{root}}$      | Root pool (general)                                   |
|                                          | $X_{\text{fine root}}$ | Fine root pool                                        |
|                                          | $X_{\text{coarse root}}$ | Coarse root pool                                    |
|                                          | $X_{\text{soil}}$      | Soil pools or processes                               |
|                                          | $X_{\text{litter}}$    | Litter pools or processes                             |
|                                          | $X_{\text{veg}}$       | Vegetation processes (general)                        |
| **Processes context**                    |                        |                                                       |
|                                          | $X_{\text{resp}}$      | Respiration processes                                 |
|                                          | $X_{\text{growth}}$    | Growth respiration                                    |
|                                          | $X_{\text{dec}}$       | Decomposition processes                               |
|                                          | $X_{\text{vol}}$       | Volatilization processes                              |
|                                          | $X_{\text{drain}}$     | Drainage processes                                    |
| **Chemical / environmental identifiers** |                        |                                                       |
|                                          | $X_{\text{org}}$       | Organic forms                                         |
|                                          | $X_{\text{mineral}}$   | Mineral forms                                         |
|                                          | $X_{\text{anaer}}$     | Anaerobic soil conditions                             |
|                                          | $X_{\text{HC}}$        | Water holding capacity                                |

Subscripts may be used in combination, e.g., $X_{\text{soil,mineral},0}$.

## Run-time Parameters

Run-time parameters can change from one run to the next, or when the model is stopped and restarted. These include initial state values and parameters related to plant physiology, soil physics, and biogeochemical cycling.

### Initial State Values

| Symbol                     | Parameter Name  | Definition                                                               | Units                                                | Notes                                                                              |
| -------------------------- | --------------- | ------------------------------------------------------------------------ | ---------------------------------------------------- | ---------------------------------------------------------------------------------- |
| $C_{\text{wood},0}$        | plantWoodInit   | Initial wood carbon                                                      | $\text{g C} \cdot \text{m}^{-2} \text{ ground area}$ | Above-ground + roots                                                               |
| $C_{\text{wood,storage}}$  |                 | Wood carbon storage pool (delta), initialized internally to 0            | $\text{g C} \cdot \text{m}^{-2} \text{ ground area}$ | Not a runtime param; $C_{\text{wood,total}} = C_{\text{wood}} + C_{\text{wood,storage}}$ |
| $LAI_0$                    | laiInit         | Initial leaf area                                                        | $\text{m}^2 \text{ leaves} \cdot \text{m}^{-2} \text{ ground area}$ | Multiply by SLW to get initial plant leaf C: $C_{\text{leaf},0} = LAI_0 \cdot SLW$ |
| $C_{\text{litter},0}$      | litterInit      | Initial litter carbon                                                    | $\text{g C} \cdot \text{m}^{-2} \text{ ground area}$ |                                                                                    |
| $C_{\text{soil},0}$        | soilInit        | Initial soil carbon                                                      | $\text{g C} \cdot \text{m}^{-2} \text{ ground area}$ |                                                                                    |
| $W_{\text{soil},0}$        | soilWFracInit   | Initial soil water content                                               | unitless                                             | Fraction of soilWHC                                                                |
| $N_{\text{org, litter},0}$ | litterOrgNInit  | Initial litter organic nitrogen content                                  | $\text{g N} \cdot \text{m}^{-2}$                     |                                                                                    |
| $N_{\text{org, soil},0}$   | soilOrgNInit    | Initial soil organic nitrogen content                                    | $\text{g N} \cdot \text{m}^{-2}$                     |                                                                                    |
| $N_{\text{min, soil},0}$   | mineralNInit    | Initial soil mineral nitrogen content                                    | $\text{g N} \cdot \text{m}^{-2}$                     |                                                                                    |
| $f_{\text{fine root},0}$   | fineRootFrac    | Fraction of `plantWoodInit` allocated to initial fine root carbon pool   | unitless                                             |                                                                                    |
| $f_{\text{coarse root},0}$ | coarseRootFrac  | Fraction of `plantWoodInit` allocated to initial coarse root carbon pool | unitless                                             |                                                                                    |
| $\mathfrak{CH_4}_{\text{soil},0}$   |                 | Initial methane concentration in the soil                                | $\text{g C} \cdot \text{m}^{-2}$                     |                                                                                    |
| $\mathfrak{N_2O}_{\text{soil},0}$   |                 | Initial nitrous oxide concentration in the soil                          | $\text{g N} \cdot \text{m}^{-2}$                     |                                                                                    |
| $W_{\text{snow},0}$        | snowInit        | Initial snow water equivalent                                            | cm water equivalent                                  |                                                                                    |

<!--not used in CCMMF

| 7 |                                          | snowInit        | Initial snow water                        | cm water equiv.                |                                                   |
-->

<!--if separating N_min into NH4 and NO3

### Initial state values 

|     | Symbol                   | Parameter Name | Definition                    | Units        | notes |
| --- | ------------------------ | -------------- | ----------------------------- | ------------ | ----- |
|     | ${NH_4}_{\text{soil},0}$ |                | Initial soil ammonium content | g N m$^{-2}$ |       |
|     | ${NO_3}_{\text{soil},0}$ |                | Initial soil nitrate content  | g N m$^{-2}$ |       |

 
-->

### Stoichiometry Parameters

| Symbol                    | Name        | Description                            | Units | Notes                                            |
| ------------------------- |-------------| -------------------------------------- | ----- | ------------------------------------------------ |
| $CN_{\textrm{wood}}$      | woodCN      | Carbon to Nitrogen ratio of wood       |       | $CN_{\textrm{coarse root}} = CN_{\textrm{wood}}$ |
| $CN_{\textrm{leaf}}$      | leafCN      | Carbon to Nitrogen ratio of leaves     |       |                                                  |
| $CN_{\textrm{fine root}}$ | fineRootCN  | Carbon to Nitrogen ratio of fine roots |       |                                                  |
| $k_\textit{CN}$           | kCN         | Decomposition CN scaling parameter     |       |                                                  |

### Photosynthesis Parameters

| Symbol                        | Parameter Name  | Definition                                                           | Units                                                                        | Notes                                                                     |
| ----------------------------- | --------------- | -------------------------------------------------------------------- | ---------------------------------------------------------------------------- | ------------------------------------------------------------------------- |
| $A_{\text{max}}$              | aMax            | Maximum net CO$_2$ assimilation rate                                 | $\text{nmol CO}_2 \cdot \text{g}^{-1} \text{ leaf} \cdot \text{s}^{-1}$ | Assuming maximum PAR, full interception, no stress |
| $f_{A_{\text{max},d}}$        | aMaxFrac        | Average daily $A_{\text{max}}$ as fraction of instantaneous          | unitless                                             | Accounts for diurnal variation in photosynthesis             |
| $R_{\text{leaf,opt}}$         | baseFolRespFrac | Basal foliar maintenance respiration as fraction of $A_{\text{max}}$ | unitless                                             |                                                           |
| $T_{\text{psn,min}}$          | psnTMin         | Minimum temperature for net photosynthesis                           | $°\text{C}$                                          |                                                           |
| $T_{\text{psn,opt}}$          | psnTOpt         | Optimum temperature for net photosynthesis                           | $°\text{C}$                                          |                                                           |
| $T_{\text{psn,max}}$          | psnTMax         | Maximum temperature for net photosynthesis                           | $°\text{C}$                                          | Calculated internally as $2 \cdot T_{\text{psn,opt}} - T_{\text{psn,min}}$ |
| $K_{\text{VPD}}$              | dVpdSlope       | Slope of VPD–photosynthesis relationship                             | $\text{kPa}^{-1}$                                    | $D_{\text{VPD}} = 1 - K_{\text{VPD}} \cdot VPD^{K_{\text{VPD,exp}}}$ |
| $K_{\text{VPD,exp}}$          | dVpdExp         | Exponent for VPD effect on photosynthesis                            | unitless                                             | $D_{\text{VPD}} = 1 - K_{\text{VPD}} \cdot VPD^{K_{\text{VPD,exp}}}$ |
| $\text{PAR}_{1/2}$            | halfSatPar      | Half-saturation point of PAR–photosynthesis relationship             | $\text{Einsteins} \cdot \text{m}^{-2} \text{ ground area} \cdot \text{day}^{-1}$ | Must match PAR units used by runtime light calculations |
| $k_{\text{atten}}$            | attenuation     | Canopy PAR extinction coefficient                                    | unitless                                             |                                                           |

### Phenology-Related Parameters

| Symbol                 | Parameter Name   | Definition                                                              | Units                                                   | Notes                                          |
| ---------------------- | ---------------- | ----------------------------------------------------------------------- | ------------------------------------------------------- | ---------------------------------------------- |
| $D_{\text{on}}$        | leafOnDay        | Day of year when leaves appear                                          | unitless                                               | day of year (1–365)                                |
| $GDD_{\text{on}}$      | gddLeafOn        | GDD threshold for leaf appearance (GDD-based phenology)                 | $°\text{C} \cdot \text{day}$                            |                                                |
| $T_{\text{on}}$        | soilTempLeafOn   | Soil temperature threshold for leaf appearance (temp-based phenology)   | $°\text{C}$                                            |                                                |
| $D_{\text{off}}$       | leafOffDay       | Day of year for leaf drop                                               | unitless                                               | day of year (1–365)                                |
| $\Delta C_{\text{leaf}}$ | leafGrowth       | Additional leaf growth at start of growing season                       | $\text{g C} \cdot \text{m}^{-2}$                        |                                                |
| $f_{\text{fall}}$      | fracLeafFall     | Additional fraction of leaves that fall at end of growing season        | unitless                                                |                                                |
| $\alpha_{\text{leaf}}$  | leafAllocation   | Fraction of $NPP$ allocated to leaf growth                              | unitless                                                |                                                |
| $K_{\text{leaf}}$      | leafTurnoverRate | Average turnover rate of leaves                                         | $\text{year}^{-1}$                                      | Converted to per-day rate internally           |

### Allocation Parameters

| Symbol                      | Parameter Name        | Definition                                                      | Units    | Notes              |
| --------------------------- | --------------------- | --------------------------------------------------------------- | -------- | ------------------ |
| $\alpha_{\text{fine root}}$ | fineRootAllocation    | Fraction of $NPP$ allocated to fine roots                       | unitless |                    |
| $\alpha_{\text{coarse root}}$ | coarseRootAllocation | Fraction of $NPP$ allocated to coarse roots                     | unitless | Calculated internally from remainder: $\alpha_{\text{coarse root}} = 1 - \alpha_{\text{leaf}} - \alpha_{\text{wood}} - \alpha_{\text{fine root}}$ |
| $\alpha_{\text{wood}}$      | woodAllocation        | Fraction of $NPP$ allocated to wood                             | unitless |                    |

### Autotrophic Respiration Parameters

| Symbol                | Parameter Name      | Definition                                                               | Units                                          | Notes                                                                                                                                              |
| --------------------- | ------------------- | ------------------------------------------------------------------------ | ---------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------- |
| $R_{\text{a,wood},0}$ | baseVegResp         | Wood maintenance respiration rate at $0^\circ C$                         | $\text{g C respired} \cdot \text{g}^{-1} \text{ plant C} \cdot \text{day}^{-1}$ | read in as per-year rate only counts plant wood C; leaves handled elsewhere (both above and below-ground: assumed for now to have same resp. rate) |
| $Q_{10v}$             | vegRespQ10          | Vegetation respiration Q10                                               | unitless                                             | Scalar determining effect of temp on veg. resp.                                                                                                    |
| $f_{\text{growth}}$    | growthRespFrac      | growth respiration as a fraction of recent mean NPP.                     | unitless                                             |
| $D_{\text{frozen}}$    | frozenSoilFolREff   | amount that foliar resp. is shutdown if soil is frozen                   | unitless                                             | 0 = full shutdown, 1 = no shutdown                                                                                                                 |
| $T_{\text{frozen}}$    | frozenSoilThreshold | soil temperature below which frozenSoilFolREff and frozenSoilEff kick in | °C                                             |                                                                                                                                                    |
| $R_{\text{fine root}}$  | baseFineRootResp    | base respiration rate of fine roots                                      | $\text{year}^{-1}$                                | per year rate                                                                                                                                      |
| $R_{\text{coarse root}}$ | baseCoarseRootResp  | base respiration rate of coarse roots                                    | $\text{year}^{-1}$                                | per year rate                                                                                                                                      |
| $Q_{10,\text{fine root}}$   | fineRootQ10         | Q10 for fine root respiration                                            | unitless                                             |                                                                                                                                                    |
| $Q_{10,\text{coarse root}}$ | coarseRootQ10       | Q10 for coarse root respiration                                          | unitless                                             |                                                                                                                                                    |

### Soil Respiration Parameters

| Symbol              | Parameter Name      | Definition                                                                          | Units                                         | Notes                                                 |
| ------------------- | ------------------- | ----------------------------------------------------------------------------------- | --------------------------------------------- | ----------------------------------------------------- |
| $K_\text{litter}$   | litterBreakdownRate | rate at which litter is converted to soil / respired at 0°C and max soil moisture   | $\text{g C broken down} \cdot \text{g}^{-1} \text{ litter C} \cdot \text{day}^{-1}$    | read in as per-year rate                              |
| $f_{\text{litter}}$ | fracLitterRespired  | of the litter broken down, fraction respired (the rest is transferred to soil pool) | unitless                                                       |                                                       |
| $K_{soil}$          | baseSoilResp        | Soil respiration rate at $0 ^{\circ}\text{C}$ and moisture saturated soil           | $\text{g C respired} \cdot \text{g}^{-1} \text{ soil C} \cdot \text{day}^{-1}$ | read in as per-year rate                              |
| $Q_{10s}$           | soilRespQ10         | Soil respiration Q10                                                                | unitless                                             | scalar determining effect of temp on soil respiration |
| $D_{\text{moisture}}$ | soilRespMoistEffect | scalar determining effect of moisture on soil resp.                                 | unitless                                             |                                                       |
| $f_{\textrm{till}}$ | tillageEff          | Effect of tillage on decomposition that exponentially decays over time              | fraction                                             | Documented in model structure; event-level term in `events.in` |

### Nitrogen Cycle Parameters

Run-time parameters support mineralization, volatilization, leaching, and
pool stoichiometry.

| Symbol               | Parameter Name      | Definition                                                                                   | Units              | Notes            |
| -------------------- | ------------------- | -------------------------------------------------------------------------------------------- | ------------------ | ---------------- |
| $K_\text{vol}$       | nVolatilizationFrac | Fraction of $N_\text{min}$ volatilized per day (modulated by temperature and moisture)       | $\text{day}^{-1}$ | Eq. (17)         |
| $f^N_{\text{leach}}$ | nLeachingFrac       | Leaching coefficient applied to $N_\text{min}$ scaled by drainage                            | $\text{day}^{-1}$ | Eq. (18)         |
| $\mathfrak{f}_{\text{fix,max}}$ | nFixFracMax         | Maximum fraction of plant N demand met by biological N fixation under low soil N            | fraction           |                                                                                    |
| $\mathfrak{K}_N$                | nFixHalfSatMinN     | Mineral N level at which fixation suppression factor $D_{N_\text{min}}$ equals 0.5          | $\text{g N} \cdot \text{m}^{-2}$ |                                                                                    |

### Moisture-Related Parameters

| Symbol                   | Parameter Name  | Definition                                                                                                             | Units                                           | Notes                                                                                                                                          |
| ------------------------ | --------------- | ---------------------------------------------------------------------------------------------------------------------- | ----------------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------- |
| $f_{\text{trans,avail}}$ | waterRemoveFrac | fraction of plant available soil water which can be removed in one day by transpiration without water stress occurring | unitless                                                 |                                                                                                                                                |
| $\mathfrak{f}_{\text{drain}}$       | waterDrainFrac  | fraction of soil water above capacity removed by drainage per day                                                     | unitless                                                 |                                                                                                                                                |
| $D_{\text{frozen,water}}$ | frozenSoilEff   | fraction of water that is available if soil is frozen (0 = none available, 1 = all still avail.)                       | unitless                                                 | if frozenSoilEff = 0, then shut down psn. even if WATER\_PSN = 0, if soil is frozen (if frozenSoilEff > 0, it has no effect if WATER\_PSN = 0) |
| $WUE$                   | wueConst        | water use efficiency constant                                                                                          | unitless                                                 |                                                                                                                                                |
| $WHC_{\text{soil}}$     | soilWHC         | soil (transpiration layer) water holding capacity                                                                      | cm                                              |                                                                                                                                                |
|                         | leafPoolDepth   | leaf (evaporative) pool rim thickness                                                                                  | mm                                              |                                                                                                                                                |
| $f_{\text{intercept}}$  | immedEvapFrac   | fraction of rain that is immediately intercepted & evaporated                                                          | unitless                                                 |                                                                                                                                                |
| $f_{\text{fastflow}}$   | fastFlowFrac    | fraction of water entering soil that goes directly to drainage                                                         | unitless                                                 |                                                                                                                                                |
| $f_a$                   | fAnoxia         | Soil wetness fraction at which oxygen diffusion begins to limit aerobic respiration                                   | unitless                                                 | Used in moisture partitioning between aerobic and anaerobic pathways (\eqref{eq:water_rh_2} in model structure)                             |
| $\eta$                  | anaerobicDecompRate | Relative anaerobic decomposition rate                                                                               | unitless                                                 | Active when anaerobic pathway is enabled; expected range $(0,1]$                                                                             |
| $\mathfrak{p}$                     | anaerobicTransExp | Methane anoxia sensitivity exponent in $D_{\text{water},CH_4}=A^p$                                                 | unitless                                                 |                                                                                                                                                |
| $M_{\text{snow}}$       | snowMelt        | rate at which snow melts                                                                                               | cm water equivalent per degree Celsius per day |                                                                                                                                                |
| $r_{d}$                 | rdConst         | scalar determining amount of aerodynamic resistance                                                                    | unitless                                                 |                                                                                                                                                |
| $r_{s,1}$               | rSoilConst1     |                                                                                                                        | unitless                                                 | soil resistance = e^(rSoilConst1 - rSoilConst2 \* W1) , where W1 = (water/soilWHC)                                                     |
| $r_{s,2}$               | rSoilConst2     |                                                                                                                        | unitless                                                 | soil resistance = e^(rSoilConst1 - rSoilConst2 \* W1) , where W1 = (water/soilWHC)                                                     |

### Tree Physiological Parameters

| Symbol                 | Parameter Name         | Definition                             | Units                | Notes                                                              |
| ---------------------- | ---------------------- | -------------------------------------- | -------------------- | ------------------------------------------------------------------ |
| $SLW$                  | leafCSpWt              | Specific leaf weight (leaf carbon per unit leaf area) | $\text{g C} \cdot \text{m}^{-2} \text{ leaf area}$ |                                                                    |
| $C_{\text{frac}}$      | cFracLeaf              | Carbon fraction of leaf dry mass                     | $\text{g leaf C} \cdot \text{g}^{-1} \text{ leaf}$ |                                                                    |
| $K_\text{wood}$        | woodTurnoverRate       | average turnover rate of woody plant C | $\text{year}^{-1}$      | converted to per-day rate internally; leaf loss handled separately |
| $K_\text{fine root}$   | fineRootTurnoverRate   | turnover of fine roots                 | $\text{year}^{-1}$      | converted to per-day rate internally                               |
| $K_\text{coarse root}$ | coarseRootTurnoverRate | turnover of coarse roots               | $\text{year}^{-1}$      | converted to per-day rate internally                               |

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

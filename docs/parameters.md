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
|                           | $Rh$       | Heterotrophic respiration (soil/litter decomposition) |
|                           | $Ra$       | Autotrophic respiration (vegetation respiration)       |
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

### Initial state values

| Row | Symbol                     | Parameter Name      | Definition                                                               | Units                                                | Notes                                                                              |
| --- | -------------------------- | ------------------- | ------------------------------------------------------------------------ | ---------------------------------------------------- | ---------------------------------------------------------------------------------- |
| 1   | $C_{\text{wood},0}$        | plantWoodInit       | Initial wood carbon                                                      | $\text{g C} \cdot \text{m}^{-2}$                      | Above-ground + roots                                                               |
| 2   | $LAI_0$                    | laiInit             | Initial leaf area index                                                  | $\text{m}^2 \text{ leaf} \cdot \text{m}^{-2} \text{ ground}$ | $C_{\text{leaf},0} = LAI_0 \cdot SLW$                                             |
| 3   | $C_{\text{litter},0}$      | litterInit          | Initial litter carbon                                                    | $\text{g C} \cdot \text{m}^{-2}$                      |                                                                                    |
| 4   | $C_{\text{soil},0}$        | soilInit            | Initial soil carbon                                                      | $\text{g C} \cdot \text{m}^{-2}$                      |                                                                                    |
| 5   | $W_{\text{litter},0}$      | litterWFracInit     | Initial litter water content (fraction of capacity)                      | unitless                                             | Fraction of $W_{\text{litter,HC}}$                                                 |
| 6   | $W_{\text{soil},0}$        | soilWFracInit       | Initial soil water content (fraction of capacity)                        | unitless                                             | Fraction of $W_{\text{soil,HC}}$                                                   |
| 7   | $N_{\text{org,litter},0}$  | nOrgLitterInit      | Initial litter organic nitrogen content                                  | $\text{g N} \cdot \text{m}^{-2}$                      |                                                                                    |
| 8   | $N_{\text{org,soil},0}$    | nOrgSoilInit        | Initial soil organic nitrogen content                                    | $\text{g N} \cdot \text{m}^{-2}$                      |                                                                                    |
| 9   | $N_{\text{min,soil},0}$    | nMinSoilInit        | Initial soil mineral nitrogen content                                    | $\text{g N} \cdot \text{m}^{-2}$                      |                                                                                    |
| 10  | $CH_{4,\text{soil},0}$     | ch4SoilInit         | Initial methane concentration in soil                                    | $\text{g C} \cdot \text{m}^{-2}$                      |                                                                                    |
| 11  | $N_{2O,\text{soil},0}$     | n2oSoilInit         | Initial nitrous oxide concentration in soil                              | $\text{g N} \cdot \text{m}^{-2}$                      |                                                                                    |
| 12  | $f_{\text{fine root},0}$   | fineRootFrac        | Fraction of $C_{\text{wood},0}$ allocated to fine root carbon            | unitless                                             |                                                                                    |
| 13  | $f_{\text{coarse root},0}$ | coarseRootFrac      | Fraction of $C_{\text{wood},0}$ allocated to coarse root carbon          | unitless                                             |                                                                                    |

### Litter Quality Parameters

| Row | Symbol                  | Parameter Name | Definition                                           | Units | Notes                    |
| --- | ----------------------- | -------------- | ---------------------------------------------------- | ----- | ------------------------ |
| 14  | $CN_{\text{litter}}$    | cnLitter       | Carbon-to-nitrogen ratio of litter                   |       |                          |
| 15  | $CN_{\text{wood}}$      | cnWood         | Carbon-to-nitrogen ratio of wood                     |       | $CN_{\text{coarse root}} = CN_{\text{wood}}$ |
| 16  | $CN_{\text{leaf}}$      | cnLeaf         | Carbon-to-nitrogen ratio of leaves                   |       |                          |
| 17  | $CN_{\text{fine root}}$ | cnFineRoot     | Carbon-to-nitrogen ratio of fine roots               |       |                          |
| 18  | $CN_{\text{coarse root}}$ | cnCoarseRoot | Carbon-to-nitrogen ratio of coarse roots             |       |                          |
| 19  | $k_{\text{CN}}$         | cnDecayScalar  | Decomposition C:N scaling parameter                  |       | Modulates decomposition rate with C:N ratio |

### Photosynthesis parameters

| Row | Symbol                        | Parameter Name  | Definition                                                           | Units                                                                        | Notes                                                                     |
| --- | ----------------------------- | --------------- | -------------------------------------------------------------------- | ---------------------------------------------------------------------------- | ------------------------------------------------------------------------- |
| 20  | $A_{\text{max}}$              | aMax            | Maximum net CO₂ assimilation rate                                    | $\mu\text{mol CO}_2 \cdot \text{g}^{-1} \text{ leaf} \cdot \text{s}^{-1}$ | Assuming maximum PAR, full interception, no stress |
| 21  | $f_{A_{\text{max},d}}$        | aMaxFrac        | Average daily $A_{\text{max}}$ as fraction of instantaneous          | unitless                                             | Accounts for diurnal variation in photosynthesis             |
| 22  | $R_{\text{leaf,opt}}$         | baseFolRespFrac | Basal foliar maintenance respiration as fraction of $A_{\text{max}}$ | unitless                                             |                                                           |
| 23  | $T_{\text{psn,min}}$          | psnTMin         | Minimum temperature for net photosynthesis                           | $°\text{C}$                                          |                                                           |
| 24  | $T_{\text{psn,opt}}$          | psnTOpt         | Optimum temperature for net photosynthesis                           | $°\text{C}$                                          |                                                           |
| 25  | $K_{\text{VPD}}$              | dVpdSlope       | Slope of VPD–photosynthesis relationship                             | $\text{kPa}^{-1}$                                    | $D_{\text{VPD}} = 1 - K_{\text{VPD}} \cdot VPD^{K_{\text{VPD,exp}}}$ |
| 26  | $K_{\text{VPD,exp}}$          | dVpdExp         | Exponent for VPD effect on photosynthesis                            | unitless                                             | $D_{\text{VPD}} = 1 - K_{\text{VPD}} \cdot VPD^{K_{\text{VPD,exp}}}$ |
| 27  | $\text{PAR}_{1/2}$            | halfSatPar      | Half-saturation point of PAR–photosynthesis relationship             | $\mu\text{mol} \cdot \text{m}^{-2} \cdot \text{s}^{-1}$ | PAR at half-maximum photosynthesis rate             |
| 28  | $k_{\text{atten}}$            | attenuation     | Canopy PAR extinction coefficient                                    | unitless                                             |                                                           |

### Phenology-related parameters

| Row | Symbol                 | Parameter Name   | Definition                                                              | Units                                                   | Notes                                          |
| --- | ---------------------- | ---------------- | ----------------------------------------------------------------------- | ------------------------------------------------------- | ---------------------------------------------- |
| 29  | $D_{\text{on}}$        | leafOnDay        | Day of year when leaves appear                                          | day of year (1–365)                                    |                                                |
| 30  |                        | gddLeafOn        | GDD threshold for leaf appearance (GDD-based phenology)                 | $°\text{C} \cdot \text{day}$                            |                                                |
| 31  |                        | soilTempLeafOn   | Soil temperature threshold for leaf appearance (temp-based phenology)   | $°\text{C}$                                            |                                                |
| 32  | $D_{\text{off}}$       | leafOffDay       | Day of year for leaf drop                                               | day of year (1–365)                                    |                                                |
| 33  |                        | leafGrowth       | Additional leaf growth at start of growing season                       | $\text{g C} \cdot \text{m}^{-2}$                        |                                                |
| 34  |                        | fracLeafFall     | Additional fraction of leaves that fall at end of growing season        | unitless                                                |                                                |
| 35  | $\alpha_{\text{leaf}}$  | leafAllocation   | Fraction of $NPP$ allocated to leaf growth                              | unitless                                                |                                                |
| 36  | $K_{\text{leaf}}$      | leafTurnoverRate | Average turnover rate of leaves                                         | $\text{year}^{-1}$                                      | Converted to per-day rate internally           |
| 37  | $LAI_{\text{max}}$     | laiMax           | Maximum leaf area index                                                 | $\text{m}^2 \text{ leaf} \cdot \text{m}^{-2} \text{ ground}$ |                                                |

### Allocation parameters

| Row | Symbol                      | Parameter Name        | Definition                                                      | Units    | Notes              |
| --- | --------------------------- | --------------------- | --------------------------------------------------------------- | -------- | ------------------ |
| 38  |                             | fineRootFrac          | Fraction of wood carbon allocated to fine roots                 | unitless |                    |
| 39  |                             | coarseRootFrac        | Fraction of wood carbon that is coarse roots                    | unitless |                    |
| 40  | $\alpha_{\text{fine root}}$ | fineRootAllocation    | Fraction of $NPP$ allocated to fine roots                       | unitless |                    |
| 41  | $\alpha_{\text{wood}}$      | woodAllocation        | Fraction of $NPP$ allocated to wood                             | unitless |                    |
| 42  |                             | fineRootExudation     | Fraction of $GPP$ from fine roots exuded to soil[^exudates]     | unitless | Pulsing parameters |
| 43  |                             | coarseRootExudation   | Fraction of $GPP$ from coarse roots exuded to soil[^exudates]   | unitless | Pulsing parameters |

[^exudates]: Fine and coarse root exudation are calculated as a fraction of $GPP$, but the exudates are subtracted from the fine and coarse root pools, respectively.

### Autotrophic respiration parameters

| Row | Symbol                   | Parameter Name      | Definition                                                               | Units                                          | Notes                                                                                                                                              |
| --- | ------------------------ | ------------------- | ------------------------------------------------------------------------ | ---------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------- |
| 44  | $Ra_{\text{wood},0}$     | baseVegResp         | Wood maintenance respiration rate at $0°\text{C}$                        | $\text{g C respired} \cdot \text{g}^{-1} \text{ plant C} \cdot \text{day}^{-1}$ | Assumes same rate for above-ground wood and roots; leaves handled separately |
| 45  | $Q_{10,\text{veg}}$      | vegRespQ10          | Vegetation respiration $Q_{10}$                                          | unitless                                       | Scalar determining temperature effect on $Ra$  |
| 46  |                          | growthRespFrac      | Growth respiration as fraction of recent mean $NPP$                      | unitless                                       |                                                |
| 47  |                          | frozenSoilFolREff   | Foliar respiration reduction factor when soil is frozen                  | unitless (0–1)                                 | 0 = full shutdown, 1 = no effect                 |
| 48  |                          | frozenSoilThreshold | Soil temperature threshold for frozen soil effects                       | $°\text{C}$                                    |                                                |
| 49  | $Ra_{\text{fine root}}$  | baseFineRootResp    | Base respiration rate of fine roots                                      | $\text{year}^{-1}$                             | Per-year rate; converted internally             |
| 50  | $Ra_{\text{coarse root}}$ | baseCoarseRootResp  | Base respiration rate of coarse roots                                    | $\text{year}^{-1}$                             | Per-year rate; converted internally             |

### Heterotrophic respiration (soil/litter) parameters

| Row | Symbol                | Parameter Name        | Definition                                                                          | Units                                         | Notes                                                 |
| --- | --------------------- | --------------------- | ----------------------------------------------------------------------------------- | --------------------------------------------- | ----------------------------------------------------- |
| 51  | $K_{\text{litter}}$   | litterBreakdownRate   | Litter breakdown rate at $0°\text{C}$ and maximum soil moisture                    | $\text{g C broken down} \cdot \text{g}^{-1} \text{ litter C} \cdot \text{day}^{-1}$ | Read in as per-year rate                      |
| 52  |                       | fracLitterRespired    | Fraction of broken-down litter that is respired (remainder → soil pool)            | unitless                                      |                                                       |
| 53  | $K_{\text{soil}}$     | baseSoilResp          | Soil respiration rate at $0°\text{C}$ and moisture-saturated conditions            | $\text{g C respired} \cdot \text{g}^{-1} \text{ soil C} \cdot \text{day}^{-1}$ | Read in as per-year rate; includes $Rh_{\text{soil}}$ |
| 54  | $Q_{10,\text{soil}}$  | soilRespQ10           | Soil respiration $Q_{10}$                                                          | unitless                                      | Scalar determining temperature effect on $Rh$       |
| 55  |                       | soilRespMoistEffect   | Soil respiration moisture effect scalar                                             | unitless                                      | Modulates $Rh$ with soil water content              |
| 56  | $f_{\text{till}}$     | tillageEff            | Effect of tillage on decomposition (exponentially decays over time)                | unitless (0–1)                                | Per-event in `events.in`; 0 = no effect             |

### Nitrogen cycle parameters

| Row | Symbol                    | Parameter Name      | Definition                                                                                   | Units                    | Notes                |
| --- | ------------------------- | ------------------- | -------------------------------------------------------------------------------------------- | ------------------------ | -------------------- |
| 57  | $K_{\text{vol}}$          | nVolatilizationFrac | Fraction of $N_{\text{min}}$ volatilized per day (modulated by temperature and moisture)    | $\text{day}^{-1}$        |                      |
| 58  | $f_{\text{leach}}^N$      | nLeachingFrac       | Leaching coefficient applied to $N_{\text{min}}$ scaled by drainage                          | $\text{day}^{-1}$        |                      |
| 59  | $f_{\text{fix,max}}$      | nFixFracMax         | Maximum fraction of plant $N$ demand that can be met by biological $N$ fixation              | unitless (0–1)           |                      |
| 60  | $K_{\text{N,fix}}$        | nFixHalfSatMinN     | Soil mineral $N$ level at which fixation suppression factor equals 0.5                       | $\text{g N} \cdot \text{m}^{-2}$ |                      |

### Methane parameters

| Row | Symbol              | Parameter Name       | Definition                                                | Units          | Notes |
| --- | ------------------- | -------------------- | --------------------------------------------------------- | --------------- | ----- |
| 61  | $K_{\text{meth}}$   | baseMicrobeResp      | Rate constant for methane production (anaerobic)          | $\text{day}^{-1}$ |       |
| 62  | $K_{\text{methox}}$ | methaneOxidationRate | Rate constant for methane oxidation                       | $\text{day}^{-1}$ |       |

### Water/moisture-related parameters

| Row | Symbol                      | Parameter Name  | Definition                                                                                                             | Units                                           | Notes                                                                                                                                          |
| --- | --------------------------- | --------------- | ---------------------------------------------------------------------------------------------------------------------- | ----------------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------- |
| 63  | $f_{\text{trans,avail}}$    | waterRemoveFrac | Fraction of plant-available soil water removable by transpiration per day without water stress                         | unitless                                        |                                                                                                                                                |
| 64  | $f_{\text{drain}}$          | waterDrainFrac  | Fraction of plant-available soil water removed by drainage per day                                                     | $\text{day}^{-1}$                               | Default 1 for well-drained soils                                                                                                               |
| 65  |                             | frozenSoilEff   | Fraction of water available when soil is frozen                                                                       | unitless (0–1)                                  | 0 = no water available; 1 = all water available; affects photosynthesis if combined with other water stress                                    |
| 66  |                             | wueConst        | Water use efficiency constant                                                                                          | unitless                                        |                                                                                                                                                |
| 67  | $W_{\text{litter,HC}}$      | litterWHC       | Litter water holding capacity (evaporative layer)                                                                      | $\text{cm}$                                     |                                                                                                                                                |
| 68  | $W_{\text{soil,HC}}$        | soilWHC         | Soil water holding capacity (transpiration layer)                                                                      | $\text{cm}$                                     |                                                                                                                                                |
| 69  | $f_{\text{intercept}}$      | immedEvapFrac   | Fraction of rainfall immediately intercepted and evaporated                                                            | unitless                                        |                                                                                                                                                |
| 70  |                             | fastFlowFrac    | Fraction of water entering soil that goes directly to drainage                                                         | unitless                                        |                                                                                                                                                |
| 71  | $f_{\text{melt}}$           | snowMelt        | Rate at which snow melts                                                                                               | $\text{cm water equiv.} \cdot °\text{C}^{-1} \cdot \text{day}^{-1}$ |                                                                                                                                                |
| 72  |                             | rdConst         | Scalar determining amount of aerodynamic resistance                                                                    | unitless                                        |                                                                                                                                                |
| 73  |                             | rSoilConst1     | Soil resistance parameter 1                                                                                            | unitless                                        | $r_{\text{soil}} = e^{rSoilConst1 - rSoilConst2 \cdot (W_{\text{litter}}/W_{\text{litter,HC}})}$                                             |
| 74  |                             | rSoilConst2     | Soil resistance parameter 2                                                                                            | unitless                                        | $r_{\text{soil}} = e^{rSoilConst1 - rSoilConst2 \cdot (W_{\text{litter}}/W_{\text{litter,HC}})}$                                             |

### Tree physiological parameters

| Row | Symbol                    | Parameter Name         | Definition                             | Units                        | Notes                                                              |
| --- | ------------------------- | ---------------------- | -------------------------------------- | ---------------------------- | ------------------------------------------------------------------ |
| 75  | $SLW$                     | leafCSpWt              | Specific leaf weight (leaf C per unit leaf area)                 | $\text{g C} \cdot \text{m}^{-2} \text{ leaf}$ |                                                                    |
| 76  | $C_{\text{frac,leaf}}$    | cFracLeaf              | Carbon fraction of leaf dry mass                                  | $\text{g C} \cdot \text{g}^{-1} \text{ leaf}$ |                                                                    |
| 77  | $K_{\text{wood}}$         | woodTurnoverRate       | Average turnover rate of woody plant carbon                       | $\text{year}^{-1}$                           | Converted to per-day rate internally; leaf loss handled separately |
| 78  | $K_{\text{fine root}}$    | fineRootTurnoverRate   | Turnover rate of fine roots                                      | $\text{year}^{-1}$                           | Converted to per-day rate internally                               |
| 79  | $K_{\text{coarse root}}$  | coarseRootTurnoverRate | Turnover rate of coarse roots                                    | $\text{year}^{-1}$                           | Converted to per-day rate internally                               |

## Hard-coded Values

| Parameter                   | Value                   | Description                                          |
| --------------------------- | ----------------------- | ---------------------------------------------------- |
| `C_WEIGHT`                  | 12.0                    | Molecular weight of carbon                           |
| `MEAN_NPP_DAYS`             | 5                       | Over how many days to keep running mean              |
| `MEAN_NPP_MAX_ENTRIES`      | `MEAN_NPP_DAYS`*50      | Assume maximum 2 data points per hour                |
| `MEAN_GPP_SOIL_DAYS`        | 5                       | Over how many days to keep running mean              |
| `MEAN_GPP_SOIL_MAX_ENTRIES` | `MEAN_GPP_SOIL_DAYS`*50 | Assume maximum 1 data point per hour                 |
| `LAMBDA`                    | 2501000                 | Latent heat of vaporization (J/kg)                   |
| `LAMBDA_S`                  | 2835000                 | Latent heat of sublimation (J/kg)                    |
| `RHO`                       | 1.3                     | Air density (kg/m³)                                  |
| `CP`                        | 1005                    | Specific heat of air (J/(kg·K))                      |
| `GAMMA`                     | 66                      | Psychrometric constant (Pa/K)                        |
| `E_STAR_SNOW`               | 0.6                     | Saturation vapor pressure at 0°C (kPa)               |
---
output:
  html_document: default
  pdf_document: default
---
# Updated SIPNET Equations

## Current SIPNET

These are the states and parameters as defined in Table 1 of Braswell et al. (2005) for the SIPNET model. 

The model description is in Appendix A but not reproduced here.

| Symbol   | Definition                                                      | Units                                     |
|----------|------------------------------------------------------------------|-------------------------------------------|
| Initial pool values                                                         |                                           |
| $C_{W,0}$ | Initial plant wood C content                                        | g C m$^{-2}$                             |
| $C_{L,0}$ | Initial plant leaf C content                                        | g C m$^{-2}$                             |
| $C_{S,0}$ | Initial soil C content                                              | g C m$^{-2}$                             |
| $W_0$    | Initial soil moisture content                                      | cm (precipitation equivalent)              |
| Photosynthesis parameters                                                   |                                           |
| $A_{\text{max}}$ | Maximum net CO2 assimilation rate                                | nmol CO2 g$^{-1}$ (leaf) s$^{-1}$     |
| $A_d$   | Avg. daily max photosynthesis as fraction of $A_{\text{max}}$       | (no units)                                 |
| $K_F$   | Foliar maintenance respiration as fraction of $A_{\text{max}}$     | (no units)                                 |
| $T_{\text{min}}$ | Minimum temperature for photosynthesis                          | $^{\circ}\text{C}$                       |
| $T_{\text{opt}}$ | Optimum temperature for photosynthesis                          | $^{\circ}\text{C}$                       |
| $K_{\text{VPD}}$ | Slope of VPD–photosynthesis relationship                       | kPa$^{-1}$                               |
| $\text{PAR}_{1/2}$ | Half saturation point of PAR–photosynthesis relationship     | Einsteins m$^{-2}$ day$^{-1}$          |
| $k$     | Canopy PAR extinction coefficient                                    | (no units)                                 |
| $D_{\text{on}}$ | Day of year for leaf out                                            | day of year                                |
| $D_{\text{off}}$ | Day of year for leaf drop                                           | day of year                                |
| $L_{\text{max}}$ | Maximum leaf area index obtained                                  | m$^2$ (leaf) m$^{-2}$ (ground)         |
| Respiration parameters                                                      |                                           |
| $K_A$   | Wood respiration rate at 0 $^{\circ}\text{C}$                          | g C g$^{-1}$ C yr$^{-1}$               |
| $Q_{10v}$ | Vegetation respiration Q10                                         | (no units)                                 |
| $K_H$   | Soil respiration rate at 0 $^{\circ}\text{C}$ and moisture-saturated soil | g C g$^{-1}$ C yr$^{-1}$               |
| $Q_{10s}$ | Soil respiration Q10                                              | (no units)                                 |
| Moisture parameters                                                         |                                           |
| $f$     | Fraction of soil water removable in 1 day                            | (no units)                                 |
| $K_{\text{WUE}}$ | VPD–WUE relationship                                           | mg CO2 kPa g$^{-1}$ H2O                  |
| $W_c$   | Soil water holding capacity                                        | cm (precipitation equivalent)              |
| Tree physiological parameters                                               |                                           |
| SLW       | Density of leaves                                                  | g m$^{-2}$                               |
| $C_{\text{frac}}$ | Fractional C content of leaves                                    | g C g$^{-1}$                             |
| $K_W$   | Turnover rate of plant wood C                                       | g C g$^{-1}$ C yr$^{-1}$               |

One additional parameter, $T_{\text{max}}$, the maximum temperature for photosynthesis, was calculated by assuming a symmetric function around the optimum photosynthetic temperature, $T_{\text{opt}}$. That is, for any values of $T_{\text{opt}}$ and $T_{\text{min}}$, $T_{\text{max}} = T_{\text{opt}} + (T_{\text{opt}} - T_{\text{min}})$. SIPNET, simplified PnET model; VPD, vapor pressure deficit; PAR, photosynthetically active radiation; WUE, water use efficiency; SLW, specific leaf weight.

### Equations

#### Rate Calculations

##### Plant Growth

##### Litter Addition

## [VERY MUCH DRAFT] Proposed updates to include CH4 and N2O

### Initial state values

- $C_{S,0}$: Soil organic carbon content (g C m$^{-2}$)
- $N_{S,0}$: Soil organic nitrogen content (g N m$^{-2}$), = $C_S \cdot CN_S$
- (C, N Litter)
- ${NH_4}_{S,0}$: Initial ammonium content (g N m$^{-2}$)
- ${NO_3}_{S,0}$: Initial nitrate nitrogen content (g N m$^{-2}$)
- ${CH_4}_S,0}$: Methane concentration in the soil (g C m$^{-2}$)
- ${N_2O}_{S,0}$: Nitrous oxide concentration in the soil (g N m$^{-2}$)
- ${W_S}_{S,0}$: Soil moisture content (cm)

### State Variables

Same as above, without '0' but not used as inputs

- $C_S$: Soil organic carbon content (g C m$^{-2}$)
- $N_S$: Soil organic nitrogen content (g N m$^{-2}$), = $C_S \cdot CN_S$
- (C, N Litter)
- ${NH_4}_S$: Initial ammonium content (g N m$^{-2}$)
- ${NO_3}_S$: Initial nitrate nitrogen content (g N m$^{-2}$)
- $CH_4$: Methane concentration in the soil (g C m$^{-2}$)
- $N_2O$: Nitrous oxide concentration in the soil (g N m$^{-2}$)
- $W_S$: Soil moisture content (cm)

### Parameters

- $K_{dec}$: Decomposition rate constant of SOM by microbial biomass (day$^{-1}$)
- $CN_{S}$: C-to-N ratio of the SOM pool
- $K_{meth}$: Rate constant for methane production under anaerobic conditions (day$^{-1}$)
- $K_{nitr}$: Rate constant for nitrification (day$^{-1}$)
- $K_{denitr}$: Rate constant for denitrification (day$^{-1}$)
- $f_{N2O_{nitr}}$: Fraction of nitrification leading to N$_2$O production
- $f_{N2O_{denitr}}$: Fraction of denitrification leading to N$_2$O production
- $K_{methox}$: Rate constant, methane oxidation (day$^{-1}$)
- $R_{min}$: Rate of mineralization (day$^{-1}$)
- $R_{nitr}$: Rate of nitrification (day$^{-1}$)
- $R_{denitr}$: Rate of denitrification (day$^{-1}$)
- $R_{meth}$: Rate of methane production (day$^{-1}$)
- $R_{dec}$: Rate of decomposition (day$^{-1}$)
- $Q_{10nitr}$: Temperature coefficient for $R_{nitr}$ (unitless)
- $Q_{10denitr}$: Temperature coefficient for $R_{denitr}$ (unitless)
- $Q_{10meth}$: Temperature coefficient for $R_{meth}$ (unitless)
- $Q_{10dec}$: Temperature coefficient for $R_{dec}$ (unitless)
- $f_x$: $x$ dependence functions (temperature, moisture, etc) 
  - $f$ also used for fractions; Original used $D$ but that is also used for day of year

### Equations

#### Temperature and Moisture Dependence 

Rates of decomposition, nitrification, denitrification, and methanogenesis are depend on temperature and soil moisture.

##### Temperature dependence $f_{T}$

Used for photosynthesis (eq A9):


$$
f_{T}=\max\left(\frac{(T_{max} - T_{air})(T_{air} - T_{min})}{\left(\frac{(T_{max} - T_{min})}{2}\right)^2}, 0\right)
$$



$$
f_{temp} = Q_{10}^{(T - 20)/10}
$$

##### Soil Moisture dependence

Might need to find a function with an optimal W$_S$ != 1

$$
f_{W} = 1 - \frac{W_{S}}{W_{c}}
$$

##### Dependence on soil anaerobic conditions

$$
d_{anaer} = \frac{W_{S}}{W_{c}}
$$

### Rates

##### Fertilization (no3, nh4, om)

$$
R_{nh4_fert} = \textrm{provided as a driver}
$$

$$
R_{no3_fert} = \textrm{provided as a driver}
$$

$$
R_{org_fert} = \textrm{provided as a driver}
$$

##### Decomposition (C$_S$ $\rightarrow$ CO$_2$)

$$
R_{dec} = K_{dec} \cdot f_{T} \cdot f_{W}
$$

##### N Mineralization (N$_S$ $\rightarrow$ NH$_4$)

$$
R_{min} = R_{dec} \cdot N_{S} \cdot f_{T} \cdot f_{W}
$$


##### Nitrification (NH$_4$ $\rightarrow$ NO$_3$ & N$_2$O )

$$
R_{nitr} = K_{nitr} \cdot NH_4 \cdot f_{T} \cdot f_{W}
$$

##### Denitrification (NO$_3$ $\rightarrow$ N$_2$O )

$$
R_{denitr} = K_{denitr} \cdot NO_3 \cdot f_{T} \cdot f_{anaer}
$$

##### Methane Production (C$_S$ $\rightarrow$ CH$_4$)

Need to modify this for to account for diffusion, ebullition, and plant transport

$$
R_{meth} = K_{meth} \cdot C_S \cdot f_{anaer}
$$

##### Methane Oxidation (CH$_4$ $\rightarrow$ CO$_2$)

$$
R_{methox} = K_{methox} \cdot CH_4 \cdot f_{T} \cdot f_{W}
$$

##### Nitrogen Fixation (addition of N$_S$??)

if a nitrogen fixing plant is present, N fixation is represented as a function of plant growth, needs to have a carbon cost to the plant 

$$
R_{fix} = K_{fix} \cdot R_{growth}
$$

### Differential Equations for Fluxes and Pools

##### Soil Litter

TBD

##### Soil Organic Carbon

$$
\frac{dC_S}{dt} = -R_{dec} \cdot C_S
$$

##### Soil Organic Nitrogen

$$
\frac{dN_S}{dt} = -R_{dec} \cdot N_S + R_{fix} + R_{updake}
$$

##### Soil Ammonium

$$
\frac{dNH_4}{dt} = R_{min} \cdot N_S + R_{nh4fert} - R_{nitr}
$$

##### Soil Nitrate 

$$
\frac{dNO_3}{dt} = R_{nitr}  + R_{nh4fert} - R_{denitr}
$$

##### Nitrous Oxide

$$
\frac{dN_2O}{dt} = f_{N2O_{nitr}} \cdot R_{nitr} + f_{N2O_{denitr}} \cdot R_{denitr}
$$

##### Methane

$$
\frac{dCH_4}{dt} = R_{meth} - R_{methox}
$$

## References

Braswell, Bobby H., William J. Sacks, Ernst Linder, and David S. Schimel. 2005. Estimating Diurnal to Annual Ecosystem Parameters by Synthesis of a Carbon Flux Model with Eddy Covariance Net Ecosystem Exchange Observations. Global Change Biology 11 (2): 335–55. https://doi.org/10.1111/j.1365-2486.2005.00897.x.


Manzoni, Stefano, and Amilcare Porporato. 2009. Soil Carbon and Nitrogen Mineralization: Theory and Models across Scales. Soil Biology and Biochemistry 41 (7): 1355–79. https://doi.org/10.1016/j.soilbio.2009.02.031.

Parton, W. J., E. A. Holland, S. J. Del Grosso, M. D. Hartman, R. E. Martin, A. R. Mosier, D. S. Ojima, and D. S. Schimel. 2001. Generalized Model for NOx  and N2O Emissions from Soils. Journal of Geophysical Research: Atmospheres 106 (D15): 17403–19. https://doi.org/10.1029/2001JD900101.



---
geometry: margin=0.5in
header-includes:
  - \usepackage{longtable}
  - \usepackage{amsmath}
---

# Model Structure

Goal: simplified biogeochemical model that is capable of simulating GHG balance, including soil carbon, CO2, CH4, and N2O flux. Key validation criteria is the ability to correctly capture the response of these pools and fluxes to changes in agronomic management practices, both current and future. 

Simplification criteria:

Start as simple as possible, add complexity as needed. When new features are considered, they should be evaluated alongside other possible model improvements that have been considered, and the overall list of project needs (see TK-Appendix, CARB Wish List table)

Note that fluxes are denoted by $F$, except that respiration is denoted by $R$ following convention and previous descriptions of SIPNET.

## Carbon Dynamics

### Photosynthesis and GPP

#### Maximum Photosynthetic Rate

$$
\text{GPP}_{\text{max}} = A_{\text{max}} \cdot A_d + R_{f0} \tag{Braswell A6}\label{eq:A6}
$$

The daily maximum gross photosynthetic rate ($\text{GPP}_{\text{max}}$) accounts for leaf-level maximum assimilation rate ($A_{\text{max}}$), a scaling factor ($A_d$), and foliar respiration ($R_{f0}$).

#### Potential Photosynthesis

$$
\text{GPP}_{\text{pot}} = \text{GPP}_{\text{max}} \cdot 
  D_{\text{temp}} \cdot 
  D_{\text{VPD}} \cdot 
  D_{\text{light}} 
  \tag{Braswell A7}\label{eq:A7}
$$

The potential gross primary production ($\text{GPP}_{\text{pot}}$) is calculated by reducing $\text{GPP}_{\text{max}}$ by temperature, vapor pressure deficit, and light.

#### Adjusted Gross Primary Production

$$
\text{GPP} = \text{GPP}_{\text{pot}} \cdot D_{\text{water}} \tag{Braswell A17}\label{eq:A17}
$$

The total adjusted gross primary production (GPP) is the product of potential GPP ($\text{GPP}_{\text{pot}}$) and the water stress factor ($D_{\text{water}}$).


#### Plant Growth

$$
\text{NPP} = \text{GPP} - R_A
$$

Net primary productivity ($\text{NPP}$) is the total carbon gain of plant biomass.

$$
\text{NPP}=\sum_{1}^{j} \frac{dC_{\text{plant,j}}}{dt}
$$

Where $j$ is leaf, wood, fine root, or coarse root. NPP is allocated to plant biomass pools in proportion to their allocation parameters $\alpha_j$.

Note that $\alpha_{fine root}, \alpha_{leaf}, \alpha_{wood}$ are specified input parameters and $\alpha_{coarse root} = 1 - \alpha_{fine root} - \alpha_{leaf} - \alpha_{wood}$.

$$
dC_{\text{plant,j}} = \text{NPP} \cdot a_j - 
  F^C_{\text{harvest,removed,j}} -
  \tag{Zobitz 3}\label{eq:Z3}
$$

In the case of annuals, perennials and trees, 


#### Plant Death

Plant death is implemented as a harvest event with the fraction of biomass transferred to litter, $f_{\text{harvest,transfer,j}}$ set to 1.

#### Wood Carbon

$$
\frac{dC_\text{wood}}{dt} = \text{GPP} - R_A - F_\text{wood litter} - L \tag{Braswell A1}\label{eq:A1}
$$

$$
\frac{dC_{\text{wood}}}{dt} = \text{GPP} - R_A - F_{\text{litter,wood}} - L
$$


Change in plant wood carbon ($C_W$) over time is determined by gross primary productivity (GPP), autotrophic respiration ($R_A$) wood litter production ($F_\text{litter,wood}$), and leaf production ($L$).

#### Leaf Carbon

$$
\frac{dC_\text{leaf}}{dt} = L - F_\text{litter,leaf} \tag{Braswell A2}\label{eq:A2}
$$

The change in plant leaf carbon ($C_\text{leaf}$) over time is given by the balance of leaf production ($L$) and leaf litter production ($F_\text{litter,leaf}$).

#### Leaf Maintenance Respiration

$$
R_\text{leaf,opt} = k_F A_{\text{max}} \tag{Braswell A5}\label{eq:A5}
$$

Where R_\text{leaf,opt} is foliar maintenance respiration at $T_\text{opt}$, proportional to the maximum photosynthetic rate ($A_{\text{max}}$) with a scaling factor $k_F$.

$$
R_\text{leaf} = R_\text{leaf,opt} \cdot D_{\text{temp,Q10,}T_\text{opt}} \tag{A18}\label{eq:A18}
$$

Actual foliar respiration ($R_\text{leaf}$) is modeled as a function of the foliar respiration rate ($R_{f0}$) at optimum temperature of leaf respiration $T_\text{opt}$ and the $Q_{10}$ temperature sensitivity factor.

#### Wood Maintenance Respiration

$$
R_\text{wood} = K_\text{wood} \cdot C_\text{wood} \cdot D_{\text{temp,Q10}} \tag{Braswell A19}\label{eq:A19}
$$

Wood maintenance respiration ($R_m$) depends on the wood carbon content ($C_W$), a scaling constant ($K_w$), and the $Q_{10}$ factor for temperature sensitivity.


#### Litter Carbon

$$
\frac{dC_\text{litter}}{dt} = 
  F^C_\text{litter} - 
  R_{H_\text{litter}}
$$

Soil litter carbon ($C_\text{litter}$) changes over time due to the addition of wood and leaf litter through turnover and loss to heterotrophic respiration ($R_{H_\text{litter}}$).

$$
R_{H_\text{litter}} = K_\text{litter} \cdot D_{\text{temp}} \cdot D_{\text{water}} \cdot C_\text{litter}
$$

$$
F^C_\text{litter} = 
  \sum_{i} K_{\text{plant,i}} \cdot C_{\text{plant,i}} +
  \sum_{j} F^C_\text{harvest,transfer,j}
$$

Where $i$ is leaf, wood, fine root, or coarse root biomass pool, and $j$ is the above or belowground biomass pool transferred to litter during harvest.

#### Soil Carbon

$$
\frac{dC_\text{soil}}{dt} = F^C_\text{litter} + F^C_\text{fert,org} - R_H \tag{Braswell A3}\label{eq:A3}
$$

The change in the SOC pool over time $\frac{dC_\text{soil}}{dt}$ is determined by the addition of litter carbon and the loss of carbon to heterotrophic respiration. This model assumes no loss of SOC to leaching or erosion.

### Heterotrophic Respiration $(C_\text{soil,litter} \rightarrow CO_2)$


$$
R_{H} = C_\text{i} \cdot K_\text{i} \cdot D_{\text{temp}} \cdot D_{\text{water}}  \cdot D_{\text{tillage,i}}
$$

$R_H$ Heterotrophic respiration of soil organic matter is a function of each carbon pool (soil or litter) $C_i$ and its associated decomposition rate $K_{C_i}$ adjusted by temperature and moisture functions, and pool specific decomposition factors.

### Methane Production $(C_\text{org,soil} \rightarrow CH_4)$

$$
R_\text{meth} = K_\text{meth} \cdot C_\text{soil} \cdot D_{\mathrm{moistur,O_2}}
$$


$$
F_{CH_4} = K_\text{meth} \cdot C_\text{soil} \cdot D_\mathrm{water, O_2} \cdot D_\text{temp}
$$

Where $F_{CH_4}$ is the methane flux modified by temperature, moisture, and a water function related to oxygen availability.

## Carbon:Nitrogen Ratio Dynamics

The carbon and nitrogen cycle are tightly coupled by the C:N ratios of plant and organic matter pools. The C:N ratio of plant biomass pools is fixed, while the C:N ratio of soil organic matter and litter pools is dynamic.

### Fixed Plant C:N Ratios

Plant biomass pools have a fixed CN ratio and are thus stoichiometrically coupled to carbon:

$$
N_i = \frac{C_i}{\mathit{CN}_{i}} \label{eq:cn_stoich}
$$

Where $i$ is the leaf, wood, fine root, or coarse root pool.

Soil organic matter and litter pools have dynamic CN that is determined below.

### Dynamic Soil Organic Matter and Litter C:N Ratios ($\mathit{CN}_{\text{soil}}$)

The change in the soil C:N ratio over time of soil and litter pools depends on the rate of change of carbon and nitrogen in the pool, normalized by the total nitrogen in the pool. This makes sense as it captures how changes in carbon and nitrogen affect their ratio.

$$
\frac{d\mathit{CN}_{\text{i}}}{dt} = \frac{1}{N_{\text{i}}} \left( \frac{dC_{\text{i}}}{dt} - \mathit{CN}_{\text{i}} \cdot \frac{dN_{\text{i}}}{dt} \right)
$$

Where $i$ is either the soil organic matter or litter pool.


## Nitrogen Dynamics

### Plant Biomass Nitrogen

Similar to the stoichiometric coupling of litter fluxes, the change in plant biomass N over time is stoichiometrically coupled to plant biomass C:

$$
\frac{dN_{\text{plant,j}}}{dt} = \frac{dC_{\text{plant,j}}}{dt} / \mathit{CN}_{\text{plant,j}} \label{eq:plant_n}
$$

Where $j$ is leaf, wood, fine root, or coarse root.


### Litter Nitrogen

The change in litter nitrogen over time, $N_\text{litter}$ is determined by inputs including leaf and wood litter, nitrogen in organic matter ammendments, and losses to mineralization:


$$
\frac{dN_{\text{litter}}}{dt} = 
  F^N_{\text{litter,wood}} + 
  F^N_{\text{litter,leaf}} +
  F^N_\text{fert,org} - 
  F^N_\text{litter,min}
$$

The flux of nitrogen from living biomass to the litter pool is calculated using equation \eqref{eq:cn_stoich}, and nitrogen from organic matter ammendments are given as inputs (TK-ref fert section).

### Soil Organic Nitrogen

$$
\frac{dN_\text{org,soil}}{dt} = 
   F^N_\text{litter} -
   F^N_\text{min,soil}
$$

The change in nitrogen pools in this model is proportional to the ratio of carbon to nitrogen in the pool. Equations for the evolution of soil and litter CN are below.

### Soil Mineral Nitrogen 

$$
\frac{dN_\text{min}}{{dt}} = 
  F^N_\text{min} +
  F^N_\text{fert,min} - 
  F^N_\mathrm{vol} - 
  F^N_\text{leach} - 
  F^N_\text{uptake}
$$

Mineralization and fertilization add to the mineral nitrogen pool, and losses include mineralization, volatilization, leaching, and plant uptake, described below:

#### N Mineralization ($F^N_\text{min}$)

$$
F^N_\text{min} = 
  \frac{R_{H\text{litter}}}{\mathit{CN}_{\text{litter}}} +
  \frac{R_H}{\mathit{CN}_{\text{soil}}}
$$

#### Nitrogen Volatilization $F^N_\text{vol}: (N_\text{min,soil} \rightarrow N_2O)$

Of models that represent N2O flux as a single process rather than a combination of nitrification and denitrification, 

The simplest way to represent NO2 flux is as a proportion of the mineral N pool $N_\text{min}$ or the N mineralization rate $F_{n min}$. For example, CLM-CN and CLM 4.0 represent N2O flux as a proportion of $N_\text{min}$ (Thornton et al 2007, TK-ref CLM 4.0). By contrast, Biome-BGC (Golinkoff et al 2010; Thornton and Rosenbloom, 2005 and https://github.com/bpbond/Biome-BGC, Golinkoff et al 2010; Thornton and Rosenbloom, 2005) represents N2O flux as a proportion of the N mineralization rate.

Because we expect N2O emissions will be dominated by fertilizer N inputs, we will start with the N_min pool size approach. This approach also has the advantage of accounting for reduced N2O flux when N is limiting (Zahele and Dalmorech 2011). 

$$
F_\mathrm{N_2O vol} = K_\text{vol} \cdot N_\text{min} \cdot D_{\text{temp}} \cdot D_{\text{water}}
$$

### Nitrogen Leaching $F^N_\text{leach}$

$$
F_{n leach} = N_\text{min} \cdot F_{water, drainage} \cdot f_{N leach}
$$

Where $F_{n leach}$ is the fraction of $N_{min}$ in soil water that is available to be leached, $F_{water, drainage}$ is drainage.

### Nitrogen Fixation $F^N_\text{fix}$

For nitrogen fixing plants, rates of symbiotic nitrogen fixation are assumed to be driven by plant growth, and are modified directly by temperature but not directly modified by water. We assume that there is no non-symbiotic N fixation.

The rate at which N is fixed is a function of the NPP of the plant and a fixed parameter $K_\text{fix}$, and is modified by temperature:

$$
F_\text{n fix} = K_\text{fix} \cdot NPP  \cdot D_{\text{temp}}
$$


### Plant Nitrogen Uptake $F^N_\text{uptake}$

Plant N is calculated as as the amount of N required to support the plant growth as the sum of changes in plant N pools from equation \eqref{eq:plant_n}:

$$
\frac{dN_\text{plant}}{dt} = \sum_{i} \frac{dN_{\text{plant,i}}}{dt}
$$

Where $i$ is leaf, wood, fine root, or coarse root.

#### Nitrogen Limitation Indicator Function $I_{\text{N limit}}$

What happens when plant N demand exceeds available N? This is N limitation, a challenging process to represent in biogeochemical models.

In the context of California's intensively managed croplands that are responsible for most agricultural GHG emissions, the impact of N limitation on GHG budgets should be minimzed. Based on agricultural economic theory, we do not expect plant growth in California's managed agricultural systems to be N limited because N fertilzer is relatively low, and the value of most specialty crops is relatively high. Likely the largest impact will be if C sequestration by woodlands becomes N limited.   

The approach to representing N limitation in SIPNET will be simple, and the primary motivation for implementing this is to avoid mass imbalance. First we will identify the presence of nitrogen limitation with an indicator variable:

$$
I_{\text{N limit}} = \begin{cases}
1, & \text{if } \frac{dN_\text{plant}}{dt} \leq N_{\text{min}} \\
0, & \text{if } \frac{dN_\text{plant}}{dt} > N_{\text{min}}
\end{cases}
$$

When $I=0$, SIPNET will throw a warning and increase autotrophic respiration to $R_A=GPP$ to stop plant growth and associated N uptake:

$$
R_A = \max(R_A, I_{\text{N limit}} \cdot GPP)
$$

This is a simplifying assumption that may warrant further investigation into the causes and impacts. If the impact of this assumption on error in GHG budgets, more complex N limitation schemes will be considered.

## Water Dynamics

### Soil Water

#### Soil Water Storage

$$
\frac{dW_{\text{soil}}}{dt} = F^W_\text{precip} + F^W_{\text{irrigation}}- F^W_\text{drainage} - F^W_\text{transpiration} \tag{Braswell A4}\label{eq:A4}
$$

The change in soil water content ($W_{\text{soil}}$) is determined by precipitation $F_{\text{precip}}$ and losses due to drainage $F_{\text{drainage}}$ and transpiration $F_{\text{transpiration}}$.

$F_{\text{precip}}$ is the precipitation rate prescribed at each time step in the `<sitename>.clim` file.

#### Drainage

The drainage term is equivalent to an overflow of the bucket: if the amount of water in the soil would exceed the water holding capacity (Wc), all water in excess of Wc is removed by drainage.

$$
F_{\text{drainage}} = \max(W_{\text{soil}} - W_{\text{WHC}}, 0) \tag{Braswell Text}\label{eq:braswell_drainage}
$$

### Transpiration

#### Water Use Efficiency (WUE)

$$
\text{WUE} = \frac{K_{\text{WUE}}}{\text{VPD}} \tag{Braswell A13}\label{eq:A13}
$$

Water Use Efficiency (WUE) is defined as the ratio of a constant $K_{\text{WUE}}$ to the vapor pressure deficit (VPD).

#### Potential Transpiration

$$
T_{\text{pot}} = \frac{\text{GPP}_{\text{pot}}}{\text{WUE}} \tag{Braswell A14}\label{eq:A14}
$$

Potential transpiration ($T_{\text{pot}}$) is calculated as the potential gross primary production ($\text{GPP}_{\text{pot}}$) divided by WUE.

#### Actual Transpiration

$$
T = \min(T_{\text{pot}}, f W) \tag{Braswell A15}\label{eq:A15}
$$

Actual transpiration ($T$) is the minimum of potential transpiration ($T_{\text{pot}}$) and a fraction ($f$) of the total soil water ($W$).

#### Water Stress Factor

$$
D_{\text{water}} = \frac{T}{T_{\text{pot}}} \tag{Braswell A16}\label{eq:A16}
$$

The water stress factor ($D_{\text{water}}$) is the ratio of actual transpiration ($T$) to potential transpiration ($T_{\text{pot}}$).


## Temperature and Moisture Dependence Functions

Metabolic processes including photosynthesis, autotrophic and heterotrophic respiration, decomposition, nitrogen volatilization, and methanogenesis are modified directly by temperature, soil moisture, and / or vapor pressure deficit.

Below is a description of these functions.

### Temperature dependence functions $D_\text{temp}$

#### Parabolic Function for Photosynthesis $D_\text{temp, A}$

Photosynthesis has a temperature optimum in the range of observed air temperatures as well as maximum and minimum temperatures of photosynthesis ($A$). SIPNET represents the temperature dependence of photosynthesis as a parabolic function. This function has a maximum at the temperature optimum, and decreases as temperature moves away from the optimum. (Eq A9 in Braswell et al. 2005).

$$
D_\text{temp,A}=\max\left(\frac{(T_{max} - T_{air})(T_{air} - T_{min})}{\left(\frac{(T_{max} - T_{min})}{2}\right)^2}, 0\right)
$$

Where $T_{\text{env}}$ may be soil or air temperature ($T_\text{soil}$ or $T_\text{air}$). 

Becuase the function is symmetric around $T_\text{opt}$, the parameters $T_{\text{min}}$ and $T_{\text{opt}}$ are provided and $T_{\text{max}}$ is calculated internally as $T_{\text{max}} = 2 \cdot T_{\text{opt}} - T_{\text{min}}$.

#### Exponential Function for Respiration $D_\text(temp,Q10)$

The temperature response of autotrophic ($R_a$) and heterotrophic ($R_H$) respiration represented as an exponential relationship using a simplified Arrhenius function.

$$
D_{\text{temp,Q10}} = Q_{10}^{\frac{(T-T_\text{opt})}{10}} \tag{Braswell A18}\label{eq:BA18}
$$

The exponential function is a simplification of the Arrhenius function in which $Q_{10}$ is the temperature sensitivity parameter, $T$ is the temperature, and $T_{\text{opt}}$ is the optimal temperature for the process set to 0 for wood and soil respiration.

We assume $T=T_\text{air}$ for leaf and wood respiration, and $T=T_\text{soil}$ for soil and root respiration. The optimal temperature for leaf respiration is the optimal temperature for photosynthesis, $T_{\text{opt}}=T_{\text{opt,A}}$, while $T_{\text{opt}}=0$ for wood, root, and soil respiration.

This function provides two ways to reduce the number of parameters in the model. Braswell et al (2005) used two $Q_{10}$ values, one for $R_A$ and one for $R_H$ and these calibrated to the same value of 1.7. By contrast, Zobitz et al (2008) used four $Q_{10}$ values, one for both leaf and wood, and one each for coarse root, fine root, and soil. Notably, these four $Q_{10}$ values ranged from 1.4 to 5.8 when SIPNET was calibrated to CO2 fluxes in a subalpine forest.

### Moisture dependence functions $D_{water}$

Moisture dependence functions are typically based on soil water content as a fraction of water holding capacity, also referred to as soil moisture in SIPNET. We will represent this fraction as $f_\text{WHC}$.

### Soil Water Content Fraction

$$
f_{\text{WHC}} = \frac{W_{\text{soil}}}{W_{\text{WHC}}}
$$

Where

- $W_{\text{soil}}$: Soil water content
- $W_{\text{WHC}}$: Soil water holding capacity

#### Water Stress Factor

$$
D_{\text{water stress}} = \frac{F_\text{trans}}{F_\text{trans, opt}} \tag{Braswell A16}\label{eq:A16}
$$

The water stress factor ($D_{\text{water}}$) is the ratio of actual transpiration ($T$) to potential transpiration ($T_{\text{pot}}$).

#### Soil Respiration Moisture Dependence ($D_{\text{water}R_H}$)

The moisture dependence of heterotrophic respiration is a linear function of soil water content when soil temperature is above freezing:

$$
D_{\text{water} R_H} = 
\begin{cases}
1, & \text{if } T_{\text{soil}} \leq 0 \\
f_{\text{WHC}} & \text{if } T_{\text{soil}} > 0
\end{cases}
$$

#### Moisture Dependence For Anaerobic Metabolism with Soil Moisture Optimum

There are many possible functions for the moisture dependence of anaerobic metabolism. The key feature is that there must be an optimum moisture level.


Lets start with a two-parameter Beta function covering the range $50 < f_{\text{WHC}} < 120$.

**Beta function**

$$
D_{\mathrm{moistur,O_2}} = (f_{WHC} - f_{WHC_\text{min}})^\beta \cdot (f_{WHC_\text{max}} - f_{WHC})^\gamma
$$

Where $\beta$ and $\gamma$ are parameters that control the shape of the curve, and can be estimated for a particular maiximum and width.

For the relationship between $N_2O$ flux and soil moisture, Wang et al (2023) suggest a Gaussian function.

**Alternative Functions** Available data suggest that there are many functions that could represent the relationship between $f_{\text{WHC}}$ and fluxes from anaerobic metabolism. 
However, it is unlikely that available data will support the choice of one over others. A model with fewer parameters would be preferred, as would one that has some theoretical justification beyond shape. For example, 
Yan et al (2018) propose a $D_\text{water}$ function that captures the transition from water to oxygen limited metabolism.

Some options considered:
- Piecewise linear function
- Gaussian function (Wang et al 2023)
- Beta
- Double exponential function
- Double logistic function
- Adapt the parabolic $D_\text{temp, A}$ from SIPNET (Braswell et al 2005)

In a study of factors affecting N2O flux from agricultural soils, Wang et al 2023 found that of pH, Bulk Density, Clay, SOC, Total N, NH4+ NO3− Temperature and $f_{WHC}$, only temperature and $f_{WHC}$ were significantly correlated with $F_\mathrm{N_2O}$.

## Agronomic Management Events

All management events are specified in the `events.in`. Each event is a separate record that includes the date of the event, the type of event, and associated parameters.

### Fertilizer and Organic Matter Additions

Additions of Mineral N, Organic N, and Organic C are represented by the fluxes $F_{N_\text{fert,min}}$, $F_{N_\text{fert,org}}, and $F_{C_\text{fert,org}}$ that are specified in the `events.in` configuration file.

Event parameters specified in the `events.in` file:

- Organic N added ($F_{N_\text{fert,org}}$)
- Organic C added ($F_{C_\text{fert,org}}$)
- Mineral N added ($F_{N_\text{fert,min}}$)

These are added to the litter C and N and mineral N pools, respectively.

Mineral N includes fertilizer supplied as NO3, NH4, and Urea-N. Urea-N is assumed to hydrolyze to ammonium and bicarbonate rapidly and is treated as a mineral N pool. This is a common assumption because of the high rate of this conversion, and is consistent the DayCent formulation (Parton et al TK-ref, other models and refs?). Only relatively recently did DayCent explicitly model Urea-N to NH4 in order to represent the impact of urease inhibitors (Gurung et al 2021) that slow down the rate.

### Tillage

To represent tillage, we define two new adjustment factors that modify the decomposition rates of litter $K_{\text{litter}}$ and soil organic matter $K_{\text{som}}$:

Event parameters from the `events.in` file:

* SOM decomposition modifier $D_{K\text{,tillage,litter}}$  
* Litter decomposition modifier $D_{K\text{,tillage,som}}$ 

These values specified as fractions (e.g. 0.2 for 20% increase in decomposition rate). They are set to 0 by default and are expected to be >0. They are set in the `events.in`, and are effective for one month after the tillage event.

$$
K_{\text{i}} = K_{\text{i}} \cdot (1+D_{K\text{,tillage,i}})
$$

Where $i$ is either litter or soil organic matter pool.

The choice of one month adjustment period is based on DayCent (Parton et al 2001).

### Planting and Emergence

Emergence is operationally defined as the date on which LAI reaches 15% of maximum.

Planting has the following parameters:

* Emergence Lag ($t_{\text{emerge}}$): The number of days from planting until emergence.
* ?? see below Plant Carbon at Emergence ($C_{\text{plant,e}}$)
* ?? see below Plant Nitrogen at Emergence ($N_{\text{plant,e}}$)

At the time of emergence, the plant carbon pools are set


Since emergence is defined as 15% LAI, plant biomass at emergence can be inferred.

First calculate the leaf carbon:

$$
C_{\text{leaf,e}} = \frac{LAI_e}{SLW}
$$

Then calculate total plant carbon at emergence:

$$
C_{\text{plant,e}} = C_{\text{leaf,e}} / \alpha_{\text{leaf}}
$$

Now carbon for each pool can be calculated using the allocation coefficients (equation \eqref{eq:Z3}), and nitrogen for each pool can be calculated using the stoichiometric ratios (equation \eqref{eq:cn_stoich}).

### Harvest

Event parameters:

* Fraction of aboveground biomass removed ($f_{\text{remove,above}}$)
* Fraction of belowground biomass removed ($f_{\text{remove,below}}$)
* Fraction of aboveground biomass transferred to litter ($f_{\text{transfer,above}}$)
* Fraction of belowground biomass transferred to litter ($f_{\text{transfer,below}}$)

A harvest event removes a fraction of aboveground (and less commonly, belowground) biomass from the system and transferrs a fraction of aboveground and typically all belowground biomass to the litter pool.

In the case of annuals and crop termination events, $f_{\text{remove,i}} + f_{\text{transfer,i}} = 1$ for each biomass pool $i$. In the case of perennials, $f_{\text{remove,i}} + f_{\text{transfer,i}} \leq 1$.

Harvest removal:

$$
F^C_{\text{harvest,removed}} = f_{\text{remove,above}} \cdot C_{\text{leaf}} + f_{\text{remove,below}} \cdot C_{\text{root}}
$$

Harvest transfer to litter:

$$
F^C_{\text{harvest,litter}} = f_{\text{transfer,above}} \cdot C_{\text{leaf}} + f_{\text{transfer,below}} \cdot C_{\text{root}}
$$

### Irrigation

Event parameters:

* Irrigation rate ($F_{\text{irrigation}}$), cm/day
* Irrigation type indicator ($I_{\text{irrigation}}$) can be added to the canopy (0) or soil (1)

For soil irrigation, the irrigation rate is added to either the precipitation flux or the soil water balance.

TK: How is irrigation applied to the canopy?


## References

Braswell, Bobby H., William J. Sacks, Ernst Linder, and David S. Schimel. 2005. Estimating Diurnal to Annual Ecosystem Parameters by Synthesis of a Carbon Flux Model with Eddy Covariance Net Ecosystem Exchange Observations. Global Change Biology 11 (2): 335–55. https://doi.org/10.1111/j.1365-2486.2005.00897.x.


Manzoni, Stefano, and Amilcare Porporato. 2009. Soil Carbon and Nitrogen Mineralization: Theory and Models across Scales. Soil Biology and Biochemistry 41 (7): 1355–79. https://doi.org/10.1016/j.soilbio.2009.02.031.

Parton, W. J., E. A. Holland, S. J. Del Grosso, M. D. Hartman, R. E. Martin, A. R. Mosier, D. S. Ojima, and D. S. Schimel. 2001. Generalized Model for NOx  and N2O Emissions from Soils. Journal of Geophysical Research: Atmospheres 106 (D15): 17403–19. https://doi.org/10.1029/2001JD900101.

Wang H, Yan Z, Ju X, Song X, Zhang J, Li S and Zhu-Barker X (2023) Quantifying nitrous oxide production rates from nitrification and denitrification under various moisture conditions in agricultural soils: Laboratory study and literature synthesis. Front. Microbiol. 13:1110151. doi: 10.3389/fmicb.2022.1110151
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

<!-- 
How to Number Equations 
(Haven't figured out how to number only the new ones automatically)
$$
Y=mX+b \tag{1} \label{eq:line}
$$

In equation \eqref{eq:line}, $m$ is the slope and $b$ is the intercept.
-->

# Model Structure

Goal: simplified biogeochemical model that is capable of simulating GHG balance, including soil carbon, $CO_2$, $CH_4$, and $N_2O$ flux. Key validation criteria is the ability to correctly capture the response of these pools and fluxes to changes in agronomic management practices, both current and future. 

### Design approach:

Start as simple as possible, add complexity as needed. When new features are considered, they should be evaluated alongside other possible model improvements that have been considered, and the overall list of project needs.

### Notes on notation:

Fluxes are denoted by $F$, except that respiration is denoted by $R$ following convention and previous descriptions of SIPNET.

Parameters and other information can be found in the Parameters documentation. <!-- or maybe combined here?-->

### Scope

This document provides an overview of the SIPNET model’s structure. It was written to 
- Consolidate the descriptions from multiple papers (notably Braswell et al 2005 and Zobitz et al 2008). 
- Provide enough detail to support the addition of agronomic events, CH4, and N2O fluxes.
- Focus on features currently in regular use.

There are multiple ways to configure the model structure, and not all model structures are listed, notably the litter quality model. 

We aim to extend the scope of this document to be more comprehensive of the regularly used features.

### $\mathfrak{Proposed \ Features}$

Proposed features are indicated using the using $\mathfrak{Fraktur Font}$. Where an entire section is new, it is only used in the section heading. Otherwise, it is used for terms within equations. As these features are implemented, the `\mathfrak{}` commands should be removed.

<!-- to remove
- remove \mathfrak{}
component out of 
-->
## Carbon Dynamics

### Maximum Photosynthetic Rate

$$
\text{GPP}_{\text{max}} = A_{\text{max}} \cdot A_d + R_{leaf,0} \tag{Braswell A6}\label{eq:A6}
$$


The daily maximum gross photosynthetic rate  $(\text{GPP}_{\text{max}})$ represents the maximum potential GPP under optimal conditions. It is modeled as the leaf-level maximum net assimilation rate  $(A_{\text{max}})$ multiplied by a scaling factor  $(A_d)$, plus foliar maintenance respiration at optimum temperature  $(R_{\text{leaf},0})$. The scaling factor $A_d$ accounts for daily variation in photosynthesis, representing the average fraction of $A_{\text{max}}$ that is realized over the course of a day. 

<!-- R_leaf,0 is included in **Gross**PP because it is part of the C flux that is directly related to the plant's photosynthetic process.

It is not immmediately clear to me why GPP is called Gross Photosynthetic Rate here, though this comes directly from Braswell et al. (2005). It seems that GPR (like its components Amax and R_leaf,0) are leaf level processes (units of /g leaf) while GPP is ecosystem scale (with units of /m2).

Assuming implementation is correct because it derived from PnET and has been used in SIPNET by many people for many years.
-->

### Potential Photosynthesis

$$
\text{GPP}_{\text{pot}} = \text{GPP}_{\text{max}} \cdot 
  D_{\text{temp}} \cdot 
  D_{\text{VPD}} \cdot 
  D_{\text{light}} 
  \tag{Braswell A7}\label{eq:A7}
$$

The potential gross primary production  $(\text{GPP}_{\text{pot}})$ is calculated by reducing $\text{GPP}_{\text{max}}$ by temperature, vapor pressure deficit, and light.

### Adjusted Gross Primary Production

$$
\text{GPP} = \text{GPP}_{\text{pot}} \cdot D_{\text{water}} \tag{Braswell A17}\label{eq:A17}
$$

The total adjusted gross primary production (GPP) is the product of potential GPP  $(\text{GPP}_{\text{pot}})$ and the water stress factor $D_{\text{water,}A}$.

The water stress factor $D_{\text{water,}A}$ is defined in equation \eqref{eq:A16} as the ratio of actual to potential transpiration, and therefore couples GPP to transpiration by reducing GPP.

### Plant Growth

$$
\text{NPP} = \text{GPP} - R_A \tag{1} \label{eq:npp}
$$

Net primary productivity  $(\text{NPP})$ is the total carbon gain of plant biomass. NPP is allocated to plant biomass pools in proportion to their allocation parameters $\alpha_i$.

$$
\text{NPP}=\sum_{1}^{i} \frac{dC_{\text{plant,}i}}{dt} \tag{2} \label{eq:npp_summ}
$$

$$\small i \in \{\text{leaf, wood, fine root, coarse root}\}$$

Note that $\alpha_i$ are specified input parameters and $\sum_i{\alpha_i} = 1$.

$$
dC_{\text{plant,}i} = \text{NPP} \cdot a_i \mathfrak{- 
  F^C_{\text{harvest,removed,}i}} - F^C_{\text{litter,}i}
  \tag{Zobitz 3}\label{eq:Z3}
$$

This results in the following constraints:
- In the case of annuals, all biomass is either harvested and removed or added to litter pools. $F^C_{\text{harvest,removed,}i}$ is calculated by \eqref{eq:harvest}.
- In the case of perennials, a fraction of the biomass remains except at the end of the perennial's life.

$$\mathfrak{
F^C_{\text{litter,}i} + F^C_{\text{harvest,removed,}i} =
\begin{cases}
1 & \text{annuals} \\
\leq 1 & \text{perennials}
\end{cases}
}$$


### Plant Death

Plant death is implemented as a harvest event with the fraction of biomass transferred to litter, $f_{\text{harvest,transfer,}i}$ set to 1.

### Wood Carbon

$$
\frac{dC_\text{wood}}{dt} = \alpha_\text{wood}\cdot\text{NPP} - F^C_\text{litter,wood} \tag{Braswell A1}\label{eq:A1}
$$

Change in plant wood carbon  $(C_W)$ over time is determined by the fraction of net primary productivity allocated to wood, and wood litter production  $(F^C_\text{litter,wood})$.


### Leaf Carbon

$$
\frac{dC_\text{leaf}}{dt} = L - F^C_\text{litter,leaf} \tag{Braswell A2}\label{eq:A2}
$$

The change in plant leaf carbon  $(C_\text{leaf})$ over time is given by the balance of leaf production  $(L)$ and leaf litter production  $(F^C_\text{litter,leaf})$.

### Leaf Maintenance Respiration

$$
R_\text{leaf,opt} = k_\text{leaf} \cdot A_{\text{max}} \cdot C_\text{leaf} \tag{Braswell A5}\label{eq:A5}
$$

Where $R_\text{leaf,opt}$ is leaf maintenance respiration at $T_\text{opt}$, proportional to the maximum photosynthetic rate $A_{\text{max}}$ with a scaling factor $k_\text{leaf}$ multiplied by the mass of leaf $C_\text{leaf}$.

$$
R_\text{leaf} = R_\text{leaf,opt} \cdot D_{\text{temp,Q10}} \tag{Braswell A18a}\label{eq:A18a}
$$

Actual foliar respiration  $(R_\text{leaf})$ is modeled as a function of the foliar respiration rate  $(R_\text{leaf,opt})$ at optimum temperature of leaf respiration $T_\text{opt}$ and the $Q_{10}$ temperature sensitivity factor.

### Wood Maintenance Respiration

$$
R_\text{wood} = K_\text{wood} \cdot C_\text{wood} \cdot D_{\text{temp,Q10}_v} \tag{Braswell A19}\label{eq:A19}
$$

Wood maintenance respiration  $(R_m)$ depends on the wood carbon content  $(C_\text{wood})$, a scaling constant  $(k_\text{wood})$, and the temperature sensitivity scaling function $D_{\text{temp,Q10}_v}$.


### Litter Carbon

The change in the litter carbon pool over time is defined by the input of new litter and the loss to decomposition:

$$
\frac{dC_\text{litter}}{dt} =
F^C_\text{litter} - F^C_{\text{decomp}}
$$

Where $F^C_\text{litter}$ is the carbon flux from plant biomass into the litter pool through senescence and harvest \eqref{eq:litter_flux}. $F^C_{\text{decomp,litter}}$ is the total carbon flux lost from the litter pool due to decomposition and includes both transfer and decomposition \eqref{eq:decomp_carbon}.

The flux of carbon from the plant to the litter pool is the sum litter produced through senescence, transfer of any biomass pools during harvest, and organic matter ammendments:

$$
F^C_\text{litter} = 
  \sum_{i} K_{\text{plant,}i} \cdot C_{\text{plant,}i} +
  \mathfrak{
    \sum_{i} F^C_{\text{harvest,transfer,}i} +
  F^C_\text{fert,org}
  } 
  \tag{3}\label{eq:litter_flux}
$$
<!-- 
_existing equation + harvest transfer and organic matter inputs
-->
$$\small i \in \{\text{leaf, wood, fine root, coarse root}\}$$

Where $K$ is the turnover rate of plant pool $i$ that controls the rate at which plant biomass is transferred to litter.

The decomposition flux from litter carbon is divided into heterotrophic respiration and carbon transfer to soil:

$$
F^C_{\text{decomp}} = R_{H,\text{litter}} + F^C_{\text{soil}} \tag{4}\label{eq:decomp_carbon}
$$

Where $R_{H_{\text{litter}}}$ is heterotrophic respiration from litter \eqref{eq:rh_litter}, and $F^C_{\text{soil}}$ is the carbon transfer from the litter pool to the soil \eqref{eq:soil_carbon}. This partitioning is based on the fraction of litter that is respired, $f_{R_H}$.

$$
R_{H_{\text{litter}}} = f_{R_H} \cdot K_\text{litter} \cdot C_\text{litter} \cdot D_{\text{temp}} \cdot D_{\text{water}R_H} \tag{5}\label{eq:rh_litter}
$$

$$
F^C_{\text{soil}} = (1 - f_{R_H}) \cdot K_\text{litter} \cdot C_\text{litter} \cdot D_{\text{temp}} \cdot D_{\text{water}R_H} \tag{6}\label{eq:soil_carbon}
$$

The rate of decomposition is a function of the litter carbon content and the decomposition rate $K_{\text{litter}}$ modified by temperature and moisture factors. $f_{R_H}$ is the fraction of litter carbon that is respired.

### Soil Carbon

$$
\frac{dC_\text{soil}}{dt} = F^C_\text{soil} - R_{H_\text{soil}} \tag{Braswell A3}\label{eq:A3}
$$

The change in the SOC pool over time $\frac{dC_\text{soil}}{dt}$ is determined by the addition of litter carbon and the loss of carbon to heterotrophic respiration. This model assumes no loss of SOC to leaching or erosion.

### Heterotrophic Respiration $(C_\text{soil,litter} \rightarrow CO_2)$

Total heterotrophic respiration is the sum of respiration from soil and litter pools:

$$
R_{H} = f_{R_H} \cdot 
  \left(\sum_j  K_j \cdot C_j 
    \mathfrak{\cdot D_{\text{tillage,}j}}
    \right) \cdot 
    D_{\text{temp}} \cdot D_{\text{water,}R_H} \cdot D_{CN} 
    \tag{7}\label{eq:rh}
$$

$$\small j \in \{\text{soil, litter}\}$$


Where heterotrophic respiration, $R_H$, is a function of each carbon pool $C_j$ and its associated decomposition rate $K_{C_j}$ adjusted by the fraction allocated to respiration, $f_{R_H}$, and the temperature, moisture, tillage, and _CN_ dependency  $(D_\star)$ functions.

### $\frak{Methane \ Production \ (C \rightarrow CH_4)}$

$$
F^C_\mathit{CH_4} = \left(\sum_{j} K_{CH_4,j} \cdot C_\text{j}\right) \cdot D_\mathrm{water, O_2} \cdot D_\text{temp}
 \tag{8}\label{eq:ch4}
$$

$$\small j \in \{\text{soil, litter}\}$$


The calculation of methane flux  $(F^C_{CH_4})$ is analagous to to that of $R_H$. It uses the same carbon pools as substrate and temperature dependence but has specific rate parameters  $(K_{\mathit{CH_4,}j})$, a moisture dependence function based on oxygen availability, and no direct dependence on tillage.

## $\frak{Carbon:Nitrogen \ Ratio \ Dynamics (CN)}$

The carbon and nitrogen cycle are tightly coupled by the C:N ratios of plant and organic matter pools. The C:N ratio of plant biomass pools is fixed, while the C:N ratio of soil organic matter and litter pools is dynamic.

### $\frak{Fixed \ Plant \ C:N \ Ratios}$

Plant biomass pools have a fixed CN ratio and are thus stoichiometrically coupled to carbon:

$$
N_i = \frac{C_i}{CN_{i}} \tag{9}\label{eq:cn_stoich}
$$

$$\small i \in \{\text{leaf, wood, fine root, coarse root}\}$$

Where $i$ is the leaf, wood, fine root, or coarse root pool. This relationship applies to both pools $C,N$ and fluxes  $(F^C, F^N)$.

Soil organic matter and litter pools have dynamic CN that is determined below.

### $\frak{Dynamic \ Soil  \ Organic \ Matter \ and \ Litter \ C:N \ Ratios}$

The change in the soil C:N ratio over time of soil and litter pools depends on the rate of change of carbon and nitrogen in the pool, normalized by the total nitrogen in the pool. This makes sense as it captures how changes in carbon and nitrogen affect their ratio.

$$
\frac{dCN_{\text{j}}}{dt} = \frac{1}{N_{\text{j}}} \left( \frac{dC_{\text{j}}}{dt} - CN_{\text{j}} \cdot \frac{dN_{\text{j}}}{dt} \right) \tag{10}\label{eq:cn}
$$

$$\small j \in \{\text{soil, litter}\}$$

### $\frak{C:N \ Dependency \ Function \ (D_{CN})}$

To represent the influence of substrate quality on decomposition rate, we add a simple dependence function $D_{CN}$.

$$
  D_{CN} = \frac{1}{1+k_CN \cdot CN} \tag{11}\label{eq:cn_dep}
$$

Where $k_CN$ is a scaling parameter that controls the sensitivity of decomposition rate to C:N ratio. This parameter represents the half-saturation constant of the Michaelis-Menten equation.

## $\frak{Nitrogen \ Dynamics (\frac{dN}{dt})}$

### $\frak{Plant \ Biomass \ Nitrogen}$

Similar to the stoichiometric coupling of litter fluxes, the change in plant biomass N over time is stoichiometrically coupled to plant biomass C:

$$
  \frac{dN_{\text{plant,}i}}{dt} = \frac{dC_{\text{plant,}i}}{dt} / CN_{\text{plant,}i} \tag{12}\label{eq:plant_n}
$$

$$\small i \in \{\text{leaf, wood, fine root, coarse root}\}$$


### $\frak{Litter \ Nitrogen}$

The change in litter nitrogen over time, $N_\text{litter}$ is determined by inputs including leaf and wood litter, nitrogen in organic matter amendments, and losses to mineralization:


$$
  \frac{dN_{\text{litter}}}{dt} = 
  \sum_{i} F^N_{\text{litter,}i} +
  F^N_\text{fert,org} - 
  F^N_\text{litter,min} \tag{13}\label{eq:litter_dndt}
$$

$$\small i \in \{\text{leaf, wood, fine root, coarse root}\}$$

The flux of nitrogen from living biomass to the litter pool is proportional to the carbon content of the biomass, based on the C:N ratio of the biomass pool \eqref{eq:cn_stoich}. Similarly, nitrogen from organic matter amendments is calculated from the carbon content and the C:N ratio of the inputs.

### $\frak{Soil \ Organic \ Nitrogen}$

$$
  \frac{dN_\text{org,soil}}{dt} = 
   F^N_\text{litter} -
   F^N_\text{soil,min} \tag{14}\label{eq:org_soil_dndt}
$$

The change in nitrogen pools in this model is proportional to the ratio of carbon to nitrogen in the pool. Equations for the evolution of soil and litter CN are below.

### $\frak{Soil \ Mineral \ Nitrogen \ F^N_\text{min}}$

Change in the mineral nitrogen pool over time is determined by inputs from mineralization and fertilization, and losses to volatilization, leaching, and plant uptake:

$$
  \frac{dN_\text{min}}{{dt}} = 
  F^N_\text{litter,min} +
  F^N_\text{soil,min} +
  F^N_\text{fix} - 
  F^N_\text{fert,min} - 
  F^N_\mathrm{vol} - 
  F^N_\text{leach} - 
  F^N_\text{uptake} 
  \tag{15}\label{eq:mineral_n_dndt}
$$

Mineralization, fertilization, and fixation add to the mineral nitrogen pool. Losses include mineralization, volatilization, leaching, and plant uptake, described below:

### $\frak{N \ Mineralization \ (F^N_\text{min})}$


Total nitrogen mineralization is proportional to the total heterotrophic respiration from soil and litter pools, divided by the C:N ratio of the pool. The effects of temperature, moisture, tillage, and C:N ratio on mineralization rate are captured in the calculation of $R_\text{H}$.


$$
  F^N_\text{min} = \sum_j \left( \frac{R_{H\text{j}}}{CN_{\text{j}}} \right) \tag{16}\label{eq:n_min}
$$

$$\small j \in \{\text{soil, litter}\}$$

### $\frak{Nitrogen \ Volatilization \ F^N_\text{vol}: (N_\text{min,soil} \rightarrow N_2O)}$


The simplest way to represent $N_2O$ flux is as a proportion of the mineral N pool $N_\text{min}$ or the N mineralization rate $F^N_{min}$. For example, CLM-CN and CLM 4.0 represent $N_2O$ flux as a proportion of $N_\text{min}$ (Thornton et al 2007, TK-ref CLM 4.0). By contrast, Biome-BGC (Golinkoff et al 2010; Thornton and Rosenbloom, 2005 and https://github.com/bpbond/Biome-BGC, Golinkoff et al 2010; Thornton and Rosenbloom, 2005) represents $N_2O$ flux as a proportion of the N mineralization rate. 

Because we expect $N_2O$ emissions will be dominated by fertilizer N inputs, we will start with the $N_\text{min}$ pool size approach. This approach also has the advantage of accounting for reduced $N_2O$ flux when N is limiting (Zahele and Dalmorech 2011).

A new fixed parameter $K_\text{vol}$ will represent the proportion of $N_\text{min}$ that is volatilized as $N_2O$.

$$
F^N_\mathrm{N_2O vol} = K_\text{vol} \cdot N_\text{min} \cdot D_{\text{temp}} \cdot D_{\text{water}R_H} \tag{17}\label{eq:n2o_vol}
$$

### $\frak{Nitrogen \ Leaching \ F^N_\text{leach}}$

$$
F^N_\text{leach} = N_\text{min} \cdot F^W_{drainage} \cdot f_{N leach} \tag{18}\label{eq:n_leach}
$$

Where $F^N_\text{leach}$ is the fraction of $N_{min}$ in soil water that is available to be leached, $F^W_{drainage}$ is drainage.

### $\frak{Nitrogen \ Fixation \ F^N_\text{fix}}$

The rate at which N is fixed is a function of the NPP of the plant and a fixed parameter $K_\text{fix}$, and is modified by temperature.

For nitrogen fixing plants, rates of symbiotic nitrogen fixation are assumed to be driven by plant growth, and also depend on temperature.

$$
F^N_\text{fix} = K_\text{fix} \cdot NPP  \cdot D_{\text{temp}} \tag{19}\label{eq:n_fix}
$$

Nitrogen fixation is represented by adding fixed nitrogen directly to the soil mineral nitrogen pool. This is a reasonable first approximation, consistent with the simplicity of the nitrogen limitation model where limitation only occurs when nitrogen demand exceeds supply. 

For nitrogen-fixing plants, most of the fixed nitrogen is directly used by the plant. It would be more complicated to model this by splitting, which could include splitting the fixed N into soil and plant pools and then meeting a portion of plant N demand with this flux.

### $\frak{Plant \ Nitrogen \ Uptake \ F^N_\text{uptake}}$

Plant N demand is the amount of N required to support plant growth. This is calculated as the sum of changes in plant N pools:

$$
F^N_\text{uptake}=\frac{dN_\text{plant}}{dt} = \sum_{i} \frac{dN_{\text{plant,}i}}{dt} \tag{20}\label{eq:plant_n_demand}
$$

$$\small i \in \{\text{leaf, wood, fine root, coarse root}\}$$

Each term in the sum is calculated according to equation \eqref{eq:plant_n}.

#### $\frak{Nitrogen \ Limitation \ Indicator \ Function \mathfrak{I_{\text{N limit}}}}$

What happens when plant N demand exceeds available N? This is N limitation, a challenging process to represent in biogeochemical models.

The initial approach to representing N limitation in SIPNET will be simple, and the primary motivation for implementing this is to avoid mass imbalance. First we will identify the presence of nitrogen limitation with an indicator variable:

$$
I_{\text{N limit}} = \begin{cases}
1, & \text{if } \frac{dN_\text{plant}}{dt} \leq N_{\text{min}} \\
0, & \text{if } \frac{dN_\text{plant}}{dt} > N_{\text{min}}
\end{cases} \tag{21}\label{eq:n_limit}
$$

When $I=0$, SIPNET will throw a warning and increase autotrophic respiration to $R_A=GPP$ to stop plant growth and associated N uptake:

$$
R_A = \max(R_A, I_{\text{N limit}} \cdot GPP) \tag{22}\label{eq:n_limit_ra}
$$

This will effectively stop plant growth and N uptake when there there is insufficient N.

We do expect N limitation to occur, including in vineyards and woodlands, but we assume that effect of nitrogen limitation on plant growth will have a relatively smaller impact on GHG budgets at the county and state scales. This is because nitrogen limitation should be rare in California's intensively managed croplands because the cost of N fertilzer is low compared to the impact of N limitation on crop yield.

If this scheme is too simple, we can adjust either the conditions under which N limitation occurs or develop an N dependency function based on the balance between plant N demand and N availability.

## Water Dynamics


### Soil Water Storage

$$
\begin{aligned}
\frac{dW_{\text{soil}}}{dt} &= f_{\text{intercept}} \cdot \Bigl( F^W_{\text{precip}} + F^W_{\text{canopy irrigation}} \Bigr)\\[1mm]
&\quad + \mathfrak{F^W_{\text{soil irrigation}}} - F^W_{\text{drainage}} - F^W_{\text{transpiration}}
\end{aligned}
\tag{Braswell A4}\label{eq:A4}
$$

The change in soil water content  $(W_{\text{soil}})$ is determined by precipitation $F^W_{\text{precip}}$ and losses due to drainage $F^W_{\text{drainage}}$ and transpiration $F^W_{\text{transpiration}}$.

$F^W_{\text{precip}}$ is the precipitation rate prescribed at each time step in the `<sitename>.clim` file and fraction of precipitation intercepted by the canopy $f_{\text{intercept}}$.



### Drainage

Under well-drained conditions, drainage occurs when soil water content  $(W_{\text{soil}})$ exceeds the soil water holding capacity  $(W_{\text{WHC}})$. Beyond this point, additional water drains off at a rate controlled by the drainage parameter $f_{\text{drain}}$. For well drained soils, this $f_{\text{drain}}=1$. Setting $f_{\text{drain}}<1$ reduced the rate of drainage, and flooding will will require a combination of a low $f_{\text{drain}}$ and sufficient size and / or frequency of $F^W_\text{irrigation}$ to maintain flooded conditions.

$$
F^W_{\text{drainage}} = f_\text{drain} \cdot \max(W_{\text{soil}} - W_{\text{WHC}}, 0) \tag{23}\label{eq:drainage}
$$

This is adapted from the original SIPNET formulation (Braswell et al 2005), adding a new parameter that controls the drainage rate.

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

Potential transpiration  $(T_{\text{pot}})$ is calculated as the potential gross primary production  $(\text{GPP}_{\text{pot}})$ divided by WUE.

#### Actual Transpiration

$$
F^W_\text{trans} = \min(F^W_\text{trans, pot}, f \cdot W_\text{soil}) \tag{Braswell A15}\label{eq:A15}
$$

Actual transpiration  $(F^W_\text{trans})$ is the minimum of potential transpiration  $(F^W_{\text{pot}})$ and the fraction  $(f)$ of the total soil water  $(W_\text{soil})$ that is removable in one day.

## Dependence Functions for Temperature and Moisture 

Metabolic processes including photosynthesis, autotrophic and heterotrophic respiration, decomposition, nitrogen volatilization, and methanogenesis are modified directly by temperature, soil moisture, and / or vapor pressure deficit.

Below is a description of these functions.

### Temperature Dependence Functions $D_\text{temp}$

#### Parabolic Function for Photosynthesis $D_\text{temp, A}$

Photosynthesis has a temperature optimum in the range of observed air temperatures as well as maximum and minimum temperatures of photosynthesis  $(A)$. SIPNET represents the temperature dependence of photosynthesis as a parabolic function. This function has a maximum at the temperature optimum, and decreases as temperature moves away from the optimum.

$$
D_\text{temp,A}=\max\left(\frac{(T_\text{max} - T_\text{air})(T_\text{air} - T_\text{min})}{\left(\frac{T_\text{max} - T_\text{min}}{2}\right)^2}, 0\right)
\tag{Braswell A9}\label{eq:A9}
$$

Where $T_{\text{env}}$ may be soil or air temperature  $(T_\text{soil}$ or $T_\text{air})$. 

Becuase the function is symmetric around $T_\text{opt}$, the parameters $T_{\text{min}}$ and $T_{\text{opt}}$ are provided and $T_{\text{max}}$ is calculated internally as $T_{\text{max}} = 2 \cdot T_{\text{opt}} - T_{\text{min}}$.

#### Exponential Function for Respiration $D_{\text(temp,Q10)}$

The temperature response of autotrophic  $(R_a)$ and heterotrophic  $(R_H)$ respiration represented as an exponential relationship using a simplified Arrhenius function.

$$
D_{\text{temp,Q10}} = Q_{10}^{\frac{(T-T_\text{opt})}{10}} \tag{Braswell A18b}\label{eq:A18b} % Defined as part of eq A18
$$

The exponential function is a simplification of the Arrhenius function in which $Q_{10}$ is the temperature sensitivity parameter, $T$ is the temperature, and $T_{\text{opt}}$ is the optimal temperature for the process set to 0 for wood and soil respiration. (Note that this is part of the equation for leaf respiration in Braswell et al. (2005).

We assume $T=T_\text{air}$ for leaf and wood respiration, and $T=T_\text{soil}$ for soil and root respiration. The optimal temperature for leaf respiration is the optimal temperature for photosynthesis, $T_{\text{opt}}=T_{\text{opt,A}}$, while $T_{\text{opt}}=0$ for wood, root, and soil respiration.

This function provides two ways to reduce the number of parameters in the model. Braswell et al (2005) used two $Q_{10}$ values, one for $R_A$ and one for $R_H$ and these calibrated to the same value of 1.7. By contrast, Zobitz et al (2008) used four $Q_{10}$ values, one for both leaf and wood, and one each for coarse root, fine root, and soil. Notably, these four $Q_{10}$ values ranged from 1.4 to 5.8 when SIPNET was calibrated to $CO_2$ fluxes in a subalpine forest.

### Moisture dependence functions $D_{water}$

Moisture dependence functions are typically based on soil water content as a fraction of water holding capacity, also referred to as soil moisture or fractional soil wetness. We will represent this fraction of soil wetness as $f_\text{WHC}$.

#### Soil Water Content Fraction

$$
f_{\text{WHC}} = \frac{W_{\text{soil}}}{W_{\text{WHC}}} 
$$

Where

- $W_{\text{soil}}$: Soil water content
- $W_{\text{WHC}}$: Soil water holding capacity

#### Water Stress Factor

$$
D_{\text{water,}A} = \frac{F^W_{\text{trans}}}{F^W_{\text{trans, pot}}} \tag{Braswell A16} \label{eq:A16}
$$

The water stress factor $(D_{\text{water,}A})$ is the ratio of actual transpiration  $(F^W_\text{trans})$ to potential transpiration  $(F^W_\text{trans, pot})$.

#### Soil Respiration Moisture Dependence  $(D_{\text{water,}R_H})$

The moisture dependence of heterotrophic respiration is a linear function of soil water content when soil temperature is above freezing:

$$
D_{\text{water} R_H} = 
\begin{cases}
1, & \text{if } T_{\text{soil}} \leq 0 \\
f_{\text{WHC}} & \text{if } T_{\text{soil}} > 0
\end{cases} \tag{24}\label{eq:water_rh}
$$

#### $\frak{Moisture \ Dependence \ For \ Anaerobic \ Metabolism \ with \ Soil \ Moisture \ Optimum}$

There are many possible functions for the moisture dependence of anaerobic metabolism. The key feature is that there must be an optimum moisture level.


Lets start with a two-parameter Beta function covering the range $50 < f_{\text{WHC}} < 120$.

**Beta function**

$$
D_{\mathrm{moistur,O_2}} = (f_{WHC} - f_{WHC_\text{min}})^\beta \cdot (f_{WHC_\text{max}} - f_{WHC})^\gamma
$$

Where $\beta$ and $\gamma$ are parameters that control the shape of the curve, and can be estimated for a particular maiximum and width.

For the relationship between $N_2O$ flux and soil moisture, Wang et al (2023) suggest a Gaussian function.

## $\frak{Agronomic \ Management \ Events}$

All management events are specified in the `events.in`. Each event is a separate record that includes the date of the event, the type of event, and associated parameters.

### $\frak{Fertilizer \ and \ Organic \ Matter \ Additions}$ 

Additions of Mineral N, Organic N, and Organic C are represented by the fluxes $F^N_{\text{fert,min}}$, $F^N_{\text{fert,org}},$ and $F^C_{\text{fert,org}}$ that are specified in the `events.in` configuration file.

Event parameters specified in the `events.in` file:

- Organic N added  $(F^N_{\text{fert,org}})$
- Organic C added  $(F^C_{\text{fert,org}})$
- Mineral N added  $(F^N_{\text{fert,min}})$

These are added to the litter C and N and mineral N pools, respectively.

Mineral N includes fertilizer supplied as NO3, NH4, and Urea-N. Urea-N is assumed to hydrolyze to ammonium and bicarbonate rapidly and is treated as a mineral N pool. This is a common assumption because of the high rate of this conversion, and is consistent the DayCent formulation (Parton et al TK-ref, other models and refs?). Only relatively recently did DayCent explicitly model Urea-N to NH4 in order to represent the impact of urease inhibitors (Gurung et al 2021) that slow down the rate.

### $\frak{Tillage}$

To represent tillage, we define two new adjustment factors that modify the decomposition rates of litter $K_{\text{litter}}$ and soil organic matter $K_{\text{som}}$:

Event parameters from the `events.in` file:

* SOM decomposition modifier $D_{K\text{,tillage,litter}}$  
* Litter decomposition modifier $D_{K\text{,tillage,som}}$ 

These values specified as fractions (e.g. 0.2 for 20% increase in decomposition rate). They are set to 0 by default and are expected to be >0. They are set in the `events.in`, and are effective for one month after the tillage event.

$$
K^{\prime}_{\text{i}} = K_{\text{i}} \cdot (1+D_{K\text{,tillage,}i})
$$

Where $i$ is either litter or soil organic matter pool, and $K^{\prime}$ is the transiently adjusted decomposition rate.

The choice of one month adjustment period is based on DayCent (Parton et al 2001).

### $\frak{Planting \ and \ Emergence}$

A planting event is defined by its emergence date and directly specifies the amount of carbon added to each of four plant carbon pools: leaf, wood, fine root, and coarse root. On the emergence date, the model initializes the plant pools with the amounts of carbon specified in the events file.

Following carbon addition, nitrogen for each pool is computed using the corresponding C:N stoichiometric ratios following equation \eqref{eq:cn_stoich}.

### $\frak{Harvest}$

A harvest event is specified by its date, the event type "harv", and the fractions of above and belowground carbon that is either transferred to litter or removed from the system.

Because a harvest event only specifies the fraction of above and belowground carbon that is removed or transferred to litter, assume that the above terms apply to leaf + wood, and below applies to fine root + coarse root.

The removed fraction is calculated as follows:

$$
F^C_{\text{harvest,removed}} = f_{\text{remove,above}} \cdot C_{\text{above}} + f_{\text{remove,below}} \cdot C_{\text{below}}
$$

The fraction transferred to litter is calculated as follows:

$$
F^C_{\text{harvest,litter}} = f_{\text{transfer,above}} \cdot C_{\text{leaf}} + f_{\text{transfer,below}} \cdot C_{\text{root}} \tag{28}\label{eq:harvest}
$$

This amount is then added to the litter flux in equation \eqref{eq:litter_flux}.

### $\frak{Irrigation}$

Event parameters:

* Irrigation rate  $(F^W_{\text{irrigation}})$, cm/day
* Irrigation type indicator  $(I_{\text{irrigation}})$:
	* Canopy irrigation (0): Water applied to the canopy, simulating rainfall.
	*	Soil irrigation (1): Water directly added to the soil.

<!--	
Note: Flooding not yet implemented
*	Flooding (2): Special case of soil irrigation, where water fully saturates the soil and maintains flooding. -->


**Canopy irrigation** is simulated in the same way as precipitation, where a fraction of irrigation is intercepted and evaporated, and the remainder is added to the soil water pool.

**Soil irrigation** adds water directly to the soil pool without interception. Flooded furrow irrigation' is a special case of soil irrigation, with a high rate of irrigation.

<!-- 
**Flooding** increases soil water to water holding capacity and then adds water equivalent to the depth of flooding. Subsequent irrigation events maintain flooding by topping off water content.

 Floodiing may also reduce the drainage parameter  $(f_{\text{drain}})$ close to zero \eq{eq:drainage}.

$$
F^W_{\text{irrigation}} = 
\begin{cases}
f_{\text{intercept}} \cdot F^W_{\text{irrigation}} & \text{canopy} \\
F^W_{\text{irrigation}} & \text{soil} \\
W_{\text{WHC}} - W_{\text{soil}} + F^W_{\text{irrigation}} & \text{flooding}
\end{cases} \tag{28}\label{eq:irrigation}
$$

-->

## References

Braswell, Bobby H., William J. Sacks, Ernst Linder, and David S. Schimel. 2005. Estimating Diurnal to Annual Ecosystem Parameters by Synthesis of a Carbon Flux Model with Eddy Covariance Net Ecosystem Exchange Observations. Global Change Biology 11 (2): 335–55. https://doi.org/10.1111/j.1365-2486.2005.00897.x.


Libohova, Z., Seybold, C., Wysocki, D., Wills, S., Schoeneberger, P., Williams, C., Lindbo, D., Stott, D. and Owens, P.R., 2018. Reevaluating the effects of soil organic matter and other properties on available water-holding capacity using the National Cooperative Soil Survey Characterization Database. Journal of soil and water conservation, 73(4), pp.411-421.

Manzoni, Stefano, and Amilcare Porporato. 2009. Soil Carbon and Nitrogen Mineralization: Theory and Models across Scales. Soil Biology and Biochemistry 41 (7): 1355–79. https://doi.org/10.1016/j.soilbio.2009.02.031.

Parton, W. J., E. A. Holland, S. J. Del Grosso, M. D. Hartman, R. E. Martin, A. R. Mosier, D. S. Ojima, and D. S. Schimel. 2001. Generalized Model for NOx  and N2O Emissions from Soils. Journal of Geophysical Research: Atmospheres 106 (D15): 17403–19. https://doi.org/10.1029/2001JD900101.

Wang H, Yan Z, Ju X, Song X, Zhang J, Li S and Zhu-Barker X (2023) Quantifying nitrous oxide production rates from nitrification and denitrification under various moisture conditions in agricultural soils: Laboratory study and literature synthesis. Front. Microbiol. 13:1110151. doi: 10.3389/fmicb.2022.1110151

Zobitz, J. M., D. J. P. Moore, W. J. Sacks, R. K. Monson, D. R. Bowling, and D. S. Schimel. 2008. “Integration of Process-Based Soil Respiration Models with Whole-Ecosystem CO2 Measurements.” Ecosystems 11 (2): 250–69. https://doi.org/10.1007/s10021-007-9120-1.

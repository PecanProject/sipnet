# Model Structure

Goal: simplified biogeochemical model that is capable of simulating GHG balance, including soil carbon, $CO_2$, $CH_4$, and $N_2O$ flux. Key validation criteria is the ability to correctly capture the response of these pools and fluxes to changes in agronomic management practices, both current and future. 

### Design approach:

Start as simple as possible, add complexity as needed. When new features are considered, they should be evaluated alongside other possible model improvements that have been considered, and the overall list of project needs.

Model state is updated in the following order:

1. Calculate fluxes — compute the model's native fluxes
2. Process events — convert events to per‑day fluxes and accumulate into fluxes.
3. Update pools — pools are updated from the accumulated fluxes and pool‑specific updates.


### Scope

This document provides an overview of the SIPNET model’s structure. It was written to 

- Consolidate the descriptions from multiple papers (notably Braswell et al 2005 and Zobitz et al 2008). 
- Provide enough detail to support the addition of agronomic events, CH4, and N2O fluxes.
- Focus on features currently in regular use.

There are multiple ways to configure the model structure, and not all model structures or components are listed. 
Implementation in source code (sipnet.c) is annotated with references to specific publications.  

#### Notes on notation:

- The general approach used to define variables and subscripts is defined in [Notation](parameters.md#sec-notation).
- Specific parameter, flux, and state definitions are documented in [Model States and Parameters](parameters.md#sec-parameters).
- $\mathfrak{Fraktur Font}$ is used to identify features that have not been implemented. This font will be removed as features are implemented.

## Carbon Dynamics

### Litter Pool

SIPNET can be run with or without a separate litter pool (LITTER_POOL=1 or 0). 
Equations in this document assume LITTER_POOL=1 unless otherwise noted.

When LITTER_POOL=0:
- Carbon fluxes that would go to the litter pool ($F^C_\text{litter}$) are routed directly to the soil carbon pool
- All decomposition occurs in the soil pool
- This affects carbon routing from harvest events, organic matter additions, plant senescence, and other processes involving $F^C_\text{litter}$
- All nitrogen cycle modeling is off (that is, NITROGEN_CYCLE=1 requires LITTER_POOL=1)

### Maximum Photosynthetic Rate

\begin{equation}
\text{GPP}_{\text{max}} = A_{\text{max}} \cdot A_d + R_{leaf,0} 
\label{eq:Braswell_A6}
\end{equation}

This is equation (A6) from Braswell et al. (2005).

The daily maximum gross photosynthetic rate $(\text{GPP}_{\text{max}})$ represents the maximum potential GPP under optimal conditions. It is modeled as the leaf-level maximum net assimilation rate  $(A_{\text{max}})$ multiplied by a scaling factor  $(A_d)$, plus foliar maintenance respiration at optimum temperature  $(R_{\text{leaf},0})$. The scaling factor $A_d$ accounts for daily variation in photosynthesis, representing the average fraction of $A_{\text{max}}$ that is realized over the course of a day. 

<!-- R_leaf,0 is included in **Gross**PP because it is part of the C flux that is directly related to the plant's photosynthetic process.

It is not immmediately clear to me why GPP is called Gross Photosynthetic Rate here, though this comes directly from Braswell et al. (2005). It seems that GPR (like its components Amax and R_leaf,0) are leaf level processes (units of /g leaf) while GPP is ecosystem scale (with units of /m2).

Assuming implementation is correct because it derived from PnET and has been used in SIPNET by many people for many years.
-->

### Potential Photosynthesis

\begin{equation}
\text{GPP}_{\text{pot}} = \text{GPP}_{\text{max}} \cdot 
  D_{\text{temp}} \cdot 
  D_{\text{VPD}} \cdot 
  D_{\text{light}} 
\label{eq:Braswell_A7}
\end{equation}

This is equation (A7) from Braswell, et al. (2005).

The potential gross primary production  $(\text{GPP}_{\text{pot}})$ is calculated by reducing $\text{GPP}_{\text{max}}$ by temperature, vapor pressure deficit, and light.

### Adjusted Gross Primary Production

\begin{equation}
\text{GPP} = \text{GPP}_{\text{pot}} \cdot D_{\text{water}} 
\label{eq:Braswell_A17}
\end{equation}

This is equation (A17) from Braswell, et al. (2005).

The total adjusted gross primary production (GPP) is the product of potential GPP  $(\text{GPP}_{\text{pot}})$ and the water stress factor $D_{\text{water,}A}$.

The water stress factor $D_{\text{water,}A}$ is defined in \eqref{eq:Braswell_A16} as the ratio of actual to potential transpiration, and therefore couples GPP to transpiration by reducing GPP.

Note that nitrogen limitation can further reduce GPP; see Sec. [Nitrogen Limitation](#nitrogen-limitation).

### Plant Growth

\begin{equation}
\text{NPP} = \text{GPP} - R_A \label{eq:npp}
\end{equation}

Net primary productivity  $(\text{NPP})$ is the total carbon gain of plant biomass. NPP is allocated to plant biomass 
pools in proportion to their allocation parameters $\alpha_i$. As in Zobitz, et al. (2008), plant growth is a determined by the running five-day mean NPP, $\overline{\text{NPP}}$.

To make explicit what contributes to autotrophic respiration, we decompose $R_A$ into maintenance and optional growth components:

\begin{equation}
R_A = R_\text{leaf} + R_\text{wood} + R_\text{fine_root} + R_\text{coarse_root} +\ R_\text{growth}
\label{eq:ra_components}
\end{equation}

Here, $R_\text{leaf}$ and $R_\text{wood}$ are maintenance respiration terms (\eqref{eq:Braswell_A18}, \eqref{eq:Braswell_A19}); 
$R_\text{fine_root}$ and $R_\text{coarse_root}$ denote root maintenance respiration (\eqref{eq:Zobitz_root_resp}); and
$R_\text{growth}$ is an optional growth respiration term. Because these components are part of $R_A$, their costs are 
subtracted from GPP before calculating NPP and before allocating NPP to plant pools.

Note that $\alpha_i$ are specified input parameters and $\sum_i{\alpha_i} = 1$.

\begin{equation}
\frac{dC_{\text{plant,}i}}{dt}
  = \alpha_i \cdot \overline{\text{NPP}}
    - F^C_{\text{harvest,removed,}i}
    - F^C_{\text{litter,}i}
\label{eq:Zobitz_3}
\end{equation}

This is equation (3) from Zobitz, et al. (2008), augmented with the harvest and litter terms. Summing over all plant pools 
shows that NPP is partitioned into biomass growth, removed harvest, and litter production.

### Plant Death

Plant death is implemented as a harvest event with the fraction of biomass transferred to litter, $f_{\text{harvest,transfer,}i}$ set to 1.

### Wood Carbon

As stated above, SIPNET uses a five-day averaged NPP when allocating gained carbon to plant growth. To implement this, 
the current timestep's net primary production (adjusted GPP - autotrophic respiration) is added to the wood carbon pool
where it acts as an _implicit_ storage pool, and all allocations from the averaged NPP are deducted from that pool.
We can represent this storage of carbon conceptually as:

\begin{equation}
NPP_\text{storage} = (GPP - R_a) - \overline{\text{NPP}}_\text{alloc}
\end{equation}

where $\overline{NPP}_\text{alloc}$ is the sum of the carbon allocated to the biomass pools as growth. Note that we do not explicitly track this storage term.

Thus, changes to wood carbon over time are determined by:

\begin{equation}
\frac{dC_\text{wood}}{dt} = NPP_\text{storage} + \alpha_\text{wood} \cdot \overline{\text{NPP}} - F^C_\text{litter,wood}
\label{eq:Braswell_A1}
\end{equation}

where $\alpha_\text{wood}\cdot\overline{\text{NPP}}$ represents the amount of carbon allocated to growth and $(F^C_\text{litter,wood})$ is the wood litter production.

This is equation (A1) from Braswell, et al. (2005), manipulated to use NPP_\text{storage}.

### Leaf Carbon

\begin{equation}
\frac{dC_\text{leaf}}{dt} = L - F^C_\text{litter,leaf} 
\label{eq:Braswell_A2}
\end{equation}

This is equation (A2) from Braswell, et al. (2005)

The change in plant leaf carbon $(C_\text{leaf})$ over time is given by the balance of leaf production $(L)$ and leaf litter production $(F^C_\text{litter,leaf})$.

**TODO:** explain $L$ in terms of $\alpha_\text{leaf}\cdot \overline{NPP}$ and leaf on/leaf off mechanics.

### Root Carbon

Both fine and coarse root carbon change in the same way as leaf carbon. Change in carbon for these pools is determined as follows, 
applied separately to fine and coarse roots:

\begin{equation}
\frac{dC_\text{i}}{dt} = \alpha_\text{i} \cdot \overline{NPP} - F^C_\text{i,root loss}
\label{eq:root_carbon}
\end{equation}

for $i \in \{\text{fine root}, \text{coarse root}\}$, where $k_\text{i,turnover}$ is the root turnover rate and 
$F^C_\text{i,root loss}$ is determined by:

\begin{equation}
F^C_\text{i,root loss} = k_\text{i,turnover} \cdot C_\text{i}
\label{eq:root_loss}
\end{equation}

### Leaf Maintenance Respiration

\begin{equation}
R_\text{leaf,opt} = k_\text{leaf} \cdot A_{\text{max}} \cdot C_\text{leaf} 
\label{eq:Braswell_A5}
\end{equation}

Where $R_\text{leaf,opt}$ is leaf maintenance respiration at $T_\text{opt}$, proportional to the maximum photosynthetic rate $A_{\text{max}}$ with a scaling factor $k_\text{leaf}$ multiplied by the mass of leaf $C_\text{leaf}$.

\begin{equation}
R_\text{leaf} = R_\text{leaf,opt} \cdot D_{\text{temp,Q10}} 
\label{eq:Braswell_A18}
\end{equation}

This is equation (A18) from Braswell, et al. (2005).

Actual foliar respiration  $(R_\text{leaf})$ is modeled as a function of the foliar respiration rate  $(R_\text{leaf,opt})$ at optimum temperature of leaf respiration $T_\text{opt}$ and the $Q_{10}$ temperature sensitivity factor.

### Wood Maintenance Respiration

\begin{equation}
R_\text{wood} = K_\text{wood} \cdot C_\text{wood} \cdot D_{\text{temp,Q10}_v}
\label{eq:Braswell_A19}
\end{equation}

This is equation (A19) from Braswell, et al. (2005).

Wood maintenance respiration $(R_m)$ depends on the wood carbon content  $(C_\text{wood})$,
a scaling constant  $(k_\text{wood})$, and the temperature sensitivity scaling function $D_{\text{temp,Q10}_v}$.

### Root Respiration

As with wood maintenance respiration, root respiration is calculated as

\begin{equation}
R_\text{i} = K_\text{} \cdot C_\text{i} \cdot D_{\text{temp,Q10}_v}
\label{eq:Zobitz_root_resp}
\end{equation}

for $i \in \{\text{fine root}, \text{coarse root}\}$, using the soil temperature for the temperature scaling function.

This is from Zobitz, et al. (2008), Sec. 5.4.2.

### Litter Carbon

The change in the litter carbon pool over time is defined by the input of new litter and losses due to decomposition:

\begin{equation}
\frac{dC_\text{litter}}{dt} = F^C_\text{litter} - F^C_{\text{decomp}}
\end{equation}

Where $F^C_\text{litter}$ is the carbon flux from aboveground plant biomass \eqref{eq:litter_flux} and $F^C_{\text{decomp}}$ 
is the total litter decomposition flux \eqref{eq:decomp_rate}. Note that belowground turnover is routed directly to the
soil carbon pool (see Soil Carbon).

$F^C_\text{litter}$ is the sum of litter produced through aboveground senescence, transfer of biomass during harvest, and organic matter amendments:

\begin{equation}
F^C_\text{litter} = 
  \sum_{i} K_{\text{plant,}i} \cdot C_{\text{plant,}i} +
  \left(
    \sum_{i} F^C_{\text{harvest,transfer,}i} +
  F^C_\text{fert,org}
  \right) 
  \label{eq:litter_flux}
\end{equation}

\begin{equation*}
\small i \in \{\text{leaf, wood}\}
\end{equation*}

Where $K_{\text{plant},i}$ is the turnover rate of plant pool $i$ that controls the rate at which plant biomass is transferred to litter.

$F^C_{\text{decomp}}$ represents the rate at which litter carbon is processed by microbial activity. Litter decomposition 
is modeled as a first-order process proportional to litter carbon content and modified by temperature and moisture:

\begin{equation}
F^C_{\text{decomp}} =
K_\text{litter} \cdot C_\text{litter} \cdot D_{\text{temp}} \cdot D_{\text{water}R_H} \cdot
D_{CN} \cdot
D_{tillage}
\label{eq:decomp_rate}
\end{equation}

The total litter decomposition flux is partitioned between heterotrophic respiration and transfer of carbon to the soil pool, satisfying the mass-balance relationship:

\begin{equation}
R_{\text{litter}} + F^C_{\text{soil,litter}} = F^C_{\text{decomp}} \label{eq:decomp_carbon}
\end{equation}

Where $R_{\text{litter}}$ is heterotrophic respiration from litter \eqref{eq:r_litter} and $F^C_{\text{soil,litter}}$ is the carbon transfer from the litter pool to the soil \eqref{eq:soil_carbon}. This partitioning is controlled by the fraction of decomposed carbon that is respired, $f_{\text{litter}}$:

\begin{equation}
R_{\text{litter}} = f_{\text{litter}} \cdot F^C_{\text{decomp}}
\label{eq:r_litter}
\end{equation}

The remainder of the decomposed litter carbon is transferred to the soil pool:

\begin{equation}
F^C_{\text{soil,litter}} =
 (1 - f_{\text{litter}}) \cdot F^C_{\text{decomp}}
\label{eq:soil_carbon}
\end{equation}

### Soil Carbon

The change in the soil organic carbon (SOC) pool over time is determined by: (i) inputs of carbon transferred from litter during decomposition, (ii) direct inputs from belowground plant turnover, and (iii) losses of carbon due to heterotrophic respiration:

\begin{equation}
\frac{dC_\text{soil}}{dt} = F^C_{\text{soil}} - R_{\text{soil}} 
\label{eq:Braswell_A3}
\end{equation}

This is equation (A3) from Braswell, et al. (2005).

Total carbon input to the soil, $F^C_{\text{soil}}$, includes both
(i) carbon transferred from the litter pool during decomposition \eqref{eq:soil_carbon} and
(ii) inputs from root turnover:

\begin{equation}
F^C_{\text{soil}} = F^C_{\text{soil,litter}} + F^C_{\text{soil,roots}}
\label{eq:soil_carbon_flux}
\end{equation}

Soil heterotrophic respiration, $R_{\text{soil}}$, is modeled as a first-order process proportional
to soil organic carbon content and modified by environmental and management factors:

\begin{equation}
R_{\text{soil}} =
  K_{dec} \cdot C_{\text{soil}} \cdot
  D_{\text{temp}} \cdot
  D_{\text{water},R_H} \cdot
  D_{CN} \cdot
  D_{\text{tillage}}
\label{eq:r_soil}
\end{equation}

SIPNET assumes no loss of SOC to leaching or erosion.

### Heterotrophic Respiration $(C_\text{soil,litter} \rightarrow \text{CO}_2)$

Total heterotrophic respiration, $R_H$, is a derived flux defined as the sum of heterotrophic respiration from soil and litter pools:

\begin{equation}
R_H = R_{\text{soil}} + R_{\text{litter}} 
\label{eq:rh}
\end{equation}

Where the litter and soil components are defined above in \eqref{eq:r_litter} and \eqref{eq:r_soil}.

For the soil pool, $R_{\text{soil}}$ is modeled as a first-order process proportional to $C_{\text{soil}}$
and modified by temperature, moisture, substrate quality (C:N), and tillage (\eqref{eq:r_soil}).

For the litter pool, litter decomposition $F^C_{\text{decomp}}$ is modeled as a first-order process proportional
to $C_{\text{litter}}$ and modified by the same dependence functions (\eqref{eq:decomp_rate}). Litter
heterotrophic respiration is then defined as a fixed fraction of this decomposition flux via $f_{\text{litter}}$
(\eqref{eq:decomp_carbon}--\eqref{eq:r_litter}), with the remainder transferred to the soil carbon pool
(\eqref{eq:soil_carbon}).

### $\frak{Methane \ Production \ (C \rightarrow CH_4)}$

\begin{equation}
F^C_\mathit{CH_4} = \left(\sum_{j} K_{CH_4,j} \cdot C_\text{j}\right) \cdot D_\mathrm{water, O_2} \cdot D_\text{temp}
\label{eq:ch4}
\end{equation}

\begin{equation*}
\small j \in \{\text{soil, litter}\}
\end{equation*}

The calculation of methane flux $(F^C_{CH_4})$ is analogous to that of $R_H$. It uses the same carbon pools as substrate
and temperature dependence but has specific rate parameters $(K_{\mathit{CH_4,}j})$, a moisture dependence function 
based on oxygen availability, and no direct dependence on tillage.

## Carbon:Nitrogen Ratio Dynamics $(CN)$

The carbon and nitrogen cycle are tightly coupled by the C:N ratios of plant and organic matter pools. The C:N ratio of plant biomass pools is fixed, while the C:N ratio of soil organic matter and litter pools is dynamic.

### Fixed Plant C:N Ratios

Plant biomass pools have a fixed CN ratio and are thus stoichiometrically coupled to carbon:

\begin{equation}
N_i = \frac{C_i}{CN_{i}}
\label{eq:cn_stoich}
\end{equation}

\begin{equation*}
\small i \in \{\text{leaf, wood, fine root, coarse root}\}
\end{equation*}

Where $i$ is the leaf, wood, fine root, or coarse root pool. This relationship applies to both pools $C,N$ and fluxes  $(F^C, F^N)$.

Soil organic matter and litter pools have dynamic CN that is determined below.

### Dynamic Soil Organic Matter and Litter C:N Ratios

In SIPNET, the C:N ratio of soil and litter pools is calculated directly from the carbon and nitrogen pools.

\begin{equation}
CN_j = \frac{C_j}{N_j}.
\end{equation}

\begin{equation*}
\small j \in \{\text{soil, litter}\}
\end{equation*}

This is used to calculate C:N-dependency $D_{CN}$ in \eqref{eq:cn_dep}.

### C:N Dependency Function $(D_{CN})$

To represent the influence of substrate quality on decomposition rate, we add a simple dependence function $D_{CN}$.
This term is used in calculation of heterotrophic respiration in \eqref{eq:rh}.

\begin{equation}
D_{CN} = \frac{k_{CN}}{k_{CN}+ CN}
\label{eq:cn_dep}
\end{equation}

Here, $k_{CN}$ is a scaling parameter that controls the sensitivity of decomposition rate to C:N ratio, with higher CN reducing the rate of decomposition.
The value $k_{CN}$ represents the C:N ratio at which decomposition is reduced by 50% ($D_{CN}= \frac{1}{2}$).

## $\frak{Nitrogen \ Dynamics (\frac{dN}{dt})}$

### $\frak{Plant \ Biomass \ Nitrogen}$

Similar to the stoichiometric coupling of litter fluxes, the change in plant biomass N over time is stoichiometrically coupled to plant biomass C:

\begin{equation}
\frac{dN_{\text{plant,}i}}{dt} = \frac{dC_{\text{plant,}i}}{dt} / CN_{\text{plant,}i} 
\label{eq:plant_n}
\end{equation}

\begin{equation*}
\small i \in \{\text{leaf, wood, fine root, coarse root}\}
\end{equation*}

### Litter Nitrogen $N_\text{litter}$

The change in litter nitrogen over time, $N_\text{litter}$ is determined by inputs including leaf and wood litter, nitrogen in organic matter amendments, and losses to mineralization as well as to transfer of organic nitrogen to the soil pool:


\begin{equation}
  \frac{dN_{\text{litter}}}{dt} = 
  \sum_{i} F^N_{\text{litter,}i} +
  F^N_\text{fert,org} - 
  F^N_\text{litter,min} -
  F^N_\text{soil}
\label{eq:litter_dndt}
\end{equation}

\begin{equation*}
\small i \in \{\text{leaf, wood}\}
\end{equation*}

Here, $F^N_{\text{litter,}i}$ includes nitrogen inputs to litter from both (i) senescence/turnover and
(ii) harvest transfers of aboveground biomass pools. The flux of nitrogen from living biomass to the litter
pool is proportional to the carbon content of the biomass, based on the C:N ratio of the biomass pool
\eqref{eq:cn_stoich}. Similarly, nitrogen from organic matter amendments is calculated from the carbon content
and the C:N ratio of the inputs.

### Soil Organic Nitrogen $N_\text{org,soil}$

The change in soil nitrogen $N_\text{org,soil}$ over time is determined by inputs including root loss, litter decomposition, and losses to mineralization:

\begin{equation}
  \frac{dN_\text{org,soil}}{dt} =
  \sum_{j} F^N_{\text{soil,}j} +
   F^N_\text{soil} - 
   F^N_\text{soil,min} 
\label{eq:org_soil_dndt}
\end{equation}

\begin{equation*}
\small j \in \{\text{fine root, coarse root}\}
\end{equation*}

$F^N_{\text{soil,}j}$ are organic nitrogen inputs to soil from belowground plant turnover and harvest
transfers of belowground biomass. 
$F^N_{\text{soil}}$ is the organic nitrogen transferred from litter to soil (calculated from
$F^C_{\text{soil}}$ in \eqref{eq:soil_carbon} based on litter C:N).
$F^N_\text{soil,min}$ is the flux from soil organic N to soil mineral N. 

### Soil Mineral Nitrogen $N_\text{min}$

The soil mineral nitrogen pool $N_\text{min}$ is defined as the amount of mineral nitrogen in the soil that is available 
for biomass use. The change in the mineral nitrogen pool over time is determined by inputs from mineralization and fertilization,
and losses to volatilization, leaching, and plant uptake:

\begin{equation}
  \frac{dN_\text{min}}{{dt}} = 
  F^N_\text{litter,min} +
  F^N_\text{soil,min} +
  F^N_\text{fert,min} - 
  F^N_\mathrm{vol} - 
  F^N_\text{leach} - 
  F^N_\text{uptake} 
\label{eq:mineral_n_dndt}
\end{equation}

Mineralization and fertilization add to the mineral nitrogen pool. Losses include volatilization, leaching, and plant uptake, described below. Fixed N enters the plant pool directly (\eqref{eq:n_fix_demand}).

### Nitrogen Mineralization $F^N_\text{min}$


Total nitrogen mineralization is proportional to the total heterotrophic respiration from soil and litter pools, divided by the C:N ratio of the pool. The effects of temperature, moisture, tillage, and C:N ratio on mineralization rate are captured in the calculation of $R_\text{H}$.


\begin{equation}
  F^N_\text{min} = \sum_j \left( \frac{R_{H\text{j}}}{CN_{\text{j}}} \right) 
\label{eq:n_min}
\end{equation}

\begin{equation*}
\small j \in \{\text{soil, litter}\}
\end{equation*}

### Nitrogen Volatilization $F^N_\text{vol}: (N_\text{min,soil} \rightarrow N_2O)$


The simplest way to represent $N_2O$ flux is as a proportion of the mineral N pool $N_\text{min}$ or the N mineralization rate $F^N_{min}$. For example, CLM-CN and CLM 4.0 represent $N_2O$ flux as a proportion of $N_\text{min}$ (Thornton et al 2007, Oleson et al. 2010). By contrast, Biome-BGC (Golinkoff et al 2010; Thornton and Rosenbloom, 2005 and https://github.com/bpbond/Biome-BGC, Golinkoff et al 2010; Thornton and Rosenbloom, 2005) represents $N_2O$ flux as a proportion of the N mineralization rate. 

Because we expect $N_2O$ emissions will be dominated by fertilizer N inputs, we will start with the $N_\text{min}$ pool size approach. This approach also has the advantage of accounting for reduced $N_2O$ flux when N is limiting (Zahele and Dalmorech 2011).

A new parameter $K_\text{vol}$ represents the first-order rate constant governing volatilization losses from the soil mineral nitrogen pool. The realized volatilization flux is proportional to $N_\text{min}$ and depends on temperature and soil moisture.

\begin{equation}
F^N_\mathrm{vol} = K_\text{vol} \cdot N_\text{min} \cdot D_{\text{temp}} \cdot D_{\text{water}R_H}
\label{eq:n_vol}
\end{equation}

### Nitrogen Leaching $F^N_\text{leach}$

\begin{equation}
F^N_\text{leach} = N_\text{min} \cdot F^W_{drainage} \cdot f_{N leach} 
\label{eq:n_leach}
\end{equation}

Where $f^N_\text{leach}$ is the fraction of $N_{min}$ in soil that is available to be leached, $F^W_{drainage}$ is drainage.

### Plant Nitrogen Demand  $F^{N}_{\text{demand}}$

Plant N demand is the amount of N required to support plant growth. This is calculated as the sum of changes in plant N pools:

\begin{equation}
F^N_\text{demand}=\frac{dN_\text{plant}}{dt} = \sum_{i} \frac{dN_{\text{plant,}i}}{dt} 
\label{eq:plant_n_demand}
\end{equation}

\begin{equation*}
\small i \in \{\text{leaf, wood, fine root, coarse root}\}
\end{equation*}

Each term in the sum is calculated according to \eqref{eq:plant_n}. Total plant N demand $F^N_\text{demand}$ is then partitioned between fixation and soil N uptake using \eqref{eq:n_fix_demand} and \eqref{eq:n_uptake_demand}.

### Nitrogen Fixation and Uptake $F^N_\text{fix}, F^N_\text{uptake}$

For N-fixing plants, symbiotic nitrogen fixation is represented as supplying a fraction of plant nitrogen demand, and is inhibited by high soil mineral N. Plant N demand is defined in \eqref{eq:plant_n_demand}.

The fraction of plant N demand met by biological N fixation is defined as:

\begin{equation}
f_\text{fix} = f_{\text{fix,max}} \cdot D_{N_\text{min}}
\label{eq:f_fix}
\end{equation}

where:

- $f_{\text{fix,max}}$ is the maximum fraction of plant N demand that can be met by fixation under low soil N (dimensionless, $0 \le f_{\text{fix,max}} \le 1$), and
- $D_{N_\text{min}}$ represents inhibition of N fixation by soil mineral N (dimensionless, $0 \le D_{N_\text{min}} \le 1$).

We use a simple down-regulation function with increasing soil mineral N:

\begin{equation}
D_{N_\text{min}} = \frac{{K_N}}{{K_N} + N_\text{min}}
\label{eq:n_fix_supp_demand}
\end{equation}

where $N_\text{min}$ is the soil mineral N pool (g N m$^{-2}$) and $K_N$ is the amount of mineral N at which fixation is reduced by half (g N m$^{-2}$).

Nitrogen fixation and soil N uptake are then partitioned from total plant N demand $F^N_\text{demand}$ (\eqref{eq:plant_n_demand}):

\begin{equation}
F^N_\text{fix} = f_\text{fix} \cdot F^N_\text{demand}
\label{eq:n_fix_demand}
\end{equation}

\begin{equation}
F^N_\text{uptake} = (1 - f_\text{fix}) \cdot F^N_\text{demand}
\label{eq:n_uptake_demand}
\end{equation}

Fixed N ($F^N_\text{fix}$) is added directly to the plant N pool via \eqref{eq:plant_n}, while $F^N_\text{uptake}$ is
removed from the soil mineral N pool in \eqref{eq:mineral_n_dndt}. If the available soil mineral N is insufficient to
supply $F^N_\text{uptake}$, then actual uptake is scaled down. See Sec. [Nitrogen Limitation](#nitrogen-limitation) for 
more information.

We do not consider free-living nonsymbiotic N fixation, which is approximately two orders of magnitude smaller (less 
than 2 kg N ha$^{-1}$ yr$^{-1}$, Cleveland et al. 1999) than crop N demand and typical N fertilization rates.

### Nitrogen Limitation {n_limit}

Nitrogen limitation occurs when plant nitrogen demand exceeds the supply of available mineral nitrogen. Plant nitrogen 
demand is diagnosed from potential biomass growth derived from five-day averaged NPP.

If this demand is greater than available mineral nitrogen, nitrogen limitation reduces plant growth.

Nitrogen limitation is applied during the flux calculation stage of the model update sequence, prior to carbon 
allocation to plant biomass pools and before any pool updates occur. N limitation is implemented as follows:

- Calculate the amount by which plant N demand exceeds available supply [^*].

- Calculate the fraction by which biomass growth must be reduced so that N demand equals supply.

- Reduce biomass growth accordingly by scaling carbon allocation to plant biomass pools.

- Calculate nitrogen uptake as the amount of N required to support the realized plant growth, based on fixed 
- stoichiometry.

- Carbon associated with the unmet growth demand is subtracted from potential GPP to maintain mass balance \eqref{eq:Braswell_A17} [^+].

[^*]: Nitrogen limitation is evaluated after accounting for biological nitrogen fixation and before mineral nitrogen 
uptake or nitrogen fertilization. Any nitrogen fertilizer inputs alleviate N limitation in subsequent time steps.

[^+]: Under nitrogen limitation, excess carbon is prevented from entering the system by down-regulating GPP. This is 
consistent with SIPNET's use of GPP as an effective ecosystem scale input rather than instantaneous leaf-level assimilation.

## Water Dynamics

### Soil Water Storage


\begin{equation}
\frac{dW_{\text{soil}}}{dt} = 
  (1 - f_{\text{intercept}})\,F^W_{\text{precip}}
  + F^W_{\text{irrig,soil}}
  - F^W_{\text{drainage}}
  - F^W_{\text{trans}}
\label{eq:Braswell_A4}
\end{equation}

This is equation (A4) from Braswell, et al. (2005).

The term $(1-f_{\text{intercept}})F^W_{\text{precip}}$ is the portion of gross precipitation that reaches the soil (i.e.
infiltration from precipitation). Intercepted water (fraction $f_{\text{intercept}}$ of precipitation or canopy‑applied
irrigation) is assumed to evaporate the same day and therefore never enters $W_{\text{soil}}$ and does not appear in 
\eqref{eq:Braswell_A4}.

### Drainage

Under well-drained conditions, drainage occurs when soil water content $(W_{\text{soil}})$ exceeds the soil water holding capacity  $(W_{\text{WHC}})$. Beyond this point, additional water drains off at a rate controlled by the drainage parameter $f_{\text{drain}}$ defined as the fraction of soil water that can be removed in one day. For well drained soils, this $f_{\text{drain}}=1$. Setting $f_{\text{drain}}<1$ reduces the rate of drainage. Flooding can be simulated by requiring a combination of a low $f_{\text{drain}}$ and sufficient $F^W_\text{irrig|precip,soil}$ to maintain flooded conditions.

\begin{equation}
F^W_{\text{drainage}} = f_\text{drain} \cdot \max(W_{\text{soil}} - W_{\text{WHC}}, 0) 
\label{eq:drainage}
\end{equation}

This is adapted from the original SIPNET formulation (Braswell et al 2005), adding a new parameter that controls the drainage rate.

### Precipitation

We define $F^W_{\text{precip}} = P$ as gross (measured) precipitation depth. The fraction reaching the soil is:

\begin{equation}
F^W_{\text{precip,soil}} = (1 - f_{\text{intercept}})\,F^W_{\text{precip}}
\end{equation}

$F^W_{\text{precip,soil}}$ is added to soil water in \eqref{eq:Braswell_A4}.

### Evapotranspiration

\begin{equation}
ET = E + T
\end{equation}

Evapotranspiration ($ET$) is calculated as the sum of evaporation ($E$) and transpiration ($T$), which are defined below:

### Evaporation

There are two components of evaporation: (1) immediate evaporation from intercepted precipitation or canopy irrigation and (2) soil surface evaporation.

**Interception (Immediate Evaporation)**

\begin{equation}
F^W_{\text{intercept,evap}} = f_{\text{intercept}}\,(F^W_{\text{precip}} + F^W_{\text{irrig,canopy}})
\end{equation}

**Soil Evaporation**

Soil evaporation is computed as:

\begin{equation}
F^W_{\text{soil,evap}} =
\frac{\rho C_p}{\gamma}\frac{1}{\lambda}
\frac{\text{VPD}_\text{soil}}{r_d + r_{\text{soil}}}
\end{equation}

where:

\begin{equation*}
r_d = \frac{\text{rdConst}}{u},
\qquad r_{\text{soil}} = \exp\!\left(r_{\text{soil},1} - r_{\text{soil},2}\frac{W_{\text{soil}}}{W_{\text{WHC}}}\right)
\end{equation*}

Negative (condensation) values are clipped to zero. If snow > 0 then $F^W_{\text{soil,evap}}=0$.

#### Evaporation

Total evaporation is calculated as the sum of intercepted water, soil evaporation, and sublimation:

\begin{equation}
E = F^W_{\text{trans}} + F^W_{\text{intercept,evap}} + F^W_{\text{soil,evap}} + F^W_{\text{sublim}}
\end{equation}

### Transpiration

#### Water Use Efficiency (WUE)

\begin{equation}
\text{WUE} = \frac{K_{\text{WUE}}}{\text{VPD}}
\label{eq:Braswell_A13}
\end{equation}

This is equation (A13) from Braswell, et al. (2005).

Water Use Efficiency (WUE) is defined as the ratio of a constant $K_{\text{WUE}}$ to the vapor pressure deficit (VPD).

#### Potential Transpiration

\begin{equation}
T_{\text{pot}} = \frac{\text{GPP}_{\text{pot}}}{\text{WUE}} 
\label{eq:Braswell_A14}
\end{equation}

This is equation (A14) from Braswell, et al. (2005).

Potential transpiration  $(T_{\text{pot}})$ is calculated as the potential gross primary production  $(\text{GPP}_{\text{pot}})$ divided by WUE.

#### Actual Transpiration

\begin{equation}
F^W_\text{trans} = \min(F^W_\text{trans, pot}, f \cdot W_\text{soil}) 
\label{eq:Braswell_A15}
\end{equation}

This is equation (A15) from Braswell, et al. (2005).

Actual transpiration  $(F^W_\text{trans})$ is the minimum of potential transpiration  $(F^W_{\text{pot}})$ and the fraction  $(f)$ of the total soil water  $(W_\text{soil})$ that is removable in one day.

## Dependence Functions for Temperature and Moisture 

Metabolic processes including photosynthesis, autotrophic and heterotrophic respiration, decomposition, nitrogen volatilization, and methanogenesis are modified directly by temperature, soil moisture, and / or vapor pressure deficit.

Below is a description of these functions.

### Temperature Dependence Functions $D_\text{temp}$

#### Parabolic Function for Photosynthesis $D_\text{temp, A}$

Photosynthesis has a temperature optimum in the range of observed air temperatures as well as maximum and minimum temperatures of photosynthesis  $(A)$. SIPNET represents the temperature dependence of photosynthesis as a parabolic function. This function has a maximum at the temperature optimum, and decreases as temperature moves away from the optimum.

\begin{equation}
D_\text{temp,A}=\max\left(\frac{(T_\text{max} - T_\text{air})(T_\text{air} - T_\text{min})}{\left(\frac{T_\text{max} - T_\text{min}}{2}\right)^2}, 0\right)
\label{eq:Braswell_A9}
\end{equation}

This is equation (A9) from Braswell, et al. (2005).

Where $T_{\text{env}}$ may be soil or air temperature  $(T_\text{soil}$ or $T_\text{air})$. 

Because the function is symmetric around $T_\text{opt}$, the parameters $T_{\text{min}}$ and $T_{\text{opt}}$ are provided and $T_{\text{max}}$ is calculated internally as $T_{\text{max}} = 2 \cdot T_{\text{opt}} - T_{\text{min}}$.

#### Exponential Function for Respiration $D_{\text(temp,Q10)}$

The temperature response of autotrophic  $(R_a)$ and heterotrophic  $(R_H)$ respiration represented as an exponential relationship using a simplified Arrhenius function.

\begin{equation}
D_{\text{temp,Q10}} = Q_{10}^{\frac{(T-T_\text{opt})}{10}} 
\label{eq:Braswell_A18b}
\end{equation}

This is from equation (A18) from Braswell, et al. (2005)

The exponential function is a simplification of the Arrhenius function in which $Q_{10}$ is the temperature sensitivity parameter, $T$ is the temperature, and $T_{\text{opt}}$ is the optimal temperature for the process set to 0 for wood and soil respiration. (Note that this is part of the equation for leaf respiration in Braswell et al. (2005)).

We assume $T=T_\text{air}$ for leaf and wood respiration, and $T=T_\text{soil}$ for soil and root respiration. The optimal temperature for leaf respiration is the optimal temperature for photosynthesis, $T_{\text{opt}}=T_{\text{opt,A}}$, while $T_{\text{opt}}=0$ for wood, root, and soil respiration.

This function provides two ways to reduce the number of parameters in the model. Braswell et al (2005) used two $Q_{10}$ values, one for $R_A$ and one for $R_H$ and these calibrated to the same value of 1.7. By contrast, Zobitz et al (2008) used four $Q_{10}$ values, one for both leaf and wood, and one each for coarse root, fine root, and soil. Notably, these four $Q_{10}$ values ranged from 1.4 to 5.8 when SIPNET was calibrated to $CO_2$ fluxes in a subalpine forest.

### Moisture dependence functions $D_{water}$

Moisture dependence functions are typically based on soil water content as a fraction of water holding capacity, also referred to as soil moisture or fractional soil wetness. We will represent this fraction of soil wetness as $f_\text{WHC}$.

#### Soil Water Content Fraction

\begin{equation}
f_{\text{WHC}} = \frac{W_{\text{soil}}}{W_{\text{WHC}}}
\end{equation}

Where

- $W_{\text{soil}}$: Soil water content
- $W_{\text{WHC}}$: Soil water holding capacity

#### Water Stress Factor

\begin{equation}
D_{\text{water,}A} = \frac{F^W_{\text{trans}}}{F^W_{\text{trans, pot}}} 
\label{eq:Braswell_A16}
\end{equation}

This is equation (A16) from Braswell, et al. (2005).

The water stress factor $(D_{\text{water,}A})$ is the ratio of actual transpiration $(F^W_\text{trans})$ to potential transpiration $(F^W_\text{trans, pot})$.

#### Soil Respiration Moisture Dependence  $(D_{\text{water,}R_H})$

The moisture dependence of heterotrophic respiration is a linear function of soil water content when soil temperature is above freezing:

\begin{equation}
D_{\text{water} R_H} = 
\begin{cases}
1, & \text{if } T_{\text{soil}} \leq 0 \\
f_{\text{WHC}} & \text{if } T_{\text{soil}} > 0
\end{cases} 
\label{eq:water_rh}
\end{equation}

#### $\frak{Moisture \ Dependence \ For \ Anaerobic \ Metabolism \ with \ Soil \ Moisture \ Optimum}$

There are many possible functions for the moisture dependence of anaerobic metabolism. The key feature is that there must be an optimum moisture level.


Lets start with a two-parameter Beta function covering the range $50 < f_{\text{WHC}} < 120$.

**Beta function**

\begin{equation}
D_{\text{water,O_2}} = (f_{WHC} - f_{WHC_\text{min}})^\beta \cdot (f_{WHC_\text{max}} - f_{WHC})^\gamma
\end{equation}

Where $\beta$ and $\gamma$ are parameters that control the shape of the curve, and can be estimated for a particular maximum and width.

For the relationship between $N_2O$ flux and soil moisture, Wang et al (2023) suggest a Gaussian function.

## Agronomic Management Events

All management events are specified in the `events.in`. Each event is a separate record that includes the 
date of the event, the type of event, and associated parameters.

### Fertilizer and Organic Matter Additions 

Additions of Mineral N, Organic N, and Organic C are added directly to their respective pools via the 
fluxes $F^N_{\text{fert,min}}$, $F^N_{\text{fert,org}},$ and $F^C_{\text{fert,org}}$ that are specified 
in the `events.in` configuration file.

Event parameters specified in the `events.in` file:
- Organic N added  $(F^N_{\text{fert,org}})$
- Organic C added  $(F^C_{\text{fert,org}})$
- Mineral N added  $(F^N_{\text{fert,min}})$

Mineral N includes fertilizer supplied as NO3, NH4, and Urea-N. Urea-N is assumed to hydrolyze to ammonium 
and bicarbonate rapidly and is treated as a mineral N pool. This is a common model assumption because of 
the fast conversion of Urea to ammonium, and is consistent with the DayCent formulation (Parton et al, 2001). 
Only relatively recently did DayCent explicitly model Urea-N to NH4 in order to represent the impact of 
urease inhibitors (Gurung et al 2021) that slow down the rate.

### Tillage

To represent the effect of tillage on decomposition rate, we define the tillage dependency function $D_{\textrm{till}}$, which is a function of a tillage effect $f_{\textrm{till}}$:

\begin{equation}
D_{\textrm{till}}(t) = 1 + f_{\textrm{till}}\cdot e^{-t/30} \label{eq:till}
\end{equation}

$f_{\textrm{till}}$ is specified in the `events.in` file, and $D_{\textrm{till}}(t)$ is multiplied by the $KC$ term in the calculation of $R_H$ (\eqref{eq:rh}).

A value of $f_{\textrm{till}}=0.2$ represents an initial 20% increase that will exponentially decay. The rate of exponential decay is 1/30 days. This rate was chosen such that $D_{\textrm{till}}$ integrates to 30, which is equivalent to DayCent’s 30‑day step function.

If multiple tillage events at times $t_z$ occur with effects $f_{\textrm{till,}z}$, they add linearly thus:

\begin{equation}
D_{\textrm{till}}(t) = 1 + \sum_{z} f_{\textrm{till,}z}\, e^{-(t-t_{z})/30},\quad t\ge t_{z}
\end{equation}

### Planting and Emergence

A planting event is defined by its emergence date and directly specifies the amount of carbon added to each of four plant carbon pools: leaf, wood, fine root, and coarse root. On the emergence date, the model initializes the plant pools with the amounts of carbon specified in the events file.

Following carbon addition, nitrogen for each pool is computed using the corresponding C:N stoichiometric ratios \eqref{eq:cn_stoich}.

### Harvest

A harvest event is specified by its date, the event type "harv", and the fractions of above and belowground carbon that
is either removed from the system or transferred to litter or soil.

Because a harvest event only specifies the fraction of above and belowground carbon that is removed or transferred, 
assume that the above terms apply to leaf + wood, and below terms apply to fine root + coarse root.

The removed fraction is calculated as follows:

\begin{equation}
F^C_{\text{harvest,removed}} = f_{\text{remove,above}} \cdot C_{\text{above}} + f_{\text{remove,below}} \cdot C_{\text{root}}
\label{eq:harvest_removed}
\end{equation}

The fraction transferred to litter is calculated as follows:

\begin{equation}
F^C_{\text{harvest,litter}} = f_{\text{transfer,above}} \cdot C_{\text{above}} 
\label{eq:harvest_litter}
\end{equation}

This amount is then added to the litter flux in \eqref{eq:litter_flux}.

Finally, the fraction transferred to soil is calculated as follows:

\begin{equation}
F^C_{\text{harvest,soil}} = f_{\text{transfer,below}} \cdot C_{\text{below}}
\label{eq:harvest_soil}
\end{equation}

Belowground harvest transfers are routed directly to the soil carbon pool and are therefore included in
the total soil carbon input flux $F^C_{\text{soil}}$ in \eqref{eq:Braswell_A3}.

### Irrigation

Event parameters:

* Irrigation rate  $(F^W_{\text{irrigation}})$, cm/day
* Irrigation type indicator  $(I_{\text{irrigation}}\in {0,1})$:
	* Canopy irrigation (0): Water applied to the canopy.
	*	Soil irrigation (1): Water directly added to the soil.

The irrigation that reaches the soil water pool is:

\begin{equation}
F^W_{\text{irrig,soil}} =
\begin{cases}
(1 - f_{\text{intercept}}) \, F^W_{\text{irrig}}, & I_{\text{irrigation}} = 0 \\
F^W_{\text{irrig}}, & I_{\text{irrigation}} = 1
\end{cases}
\label{eq:irrig_soil}
\end{equation}

Irrigation that is immediately evaporated:

\begin{equation}
F^W_{\text{irrig,evap}} =
\begin{cases}
f_{\text{intercept}} \, F^W_{\text{irrig}}, & I_{\text{irrigation}} = 0 \\
0, & I_{\text{irrigation}} = 1
\end{cases}
\label{eq:irrig_evap}
\end{equation}

<!-- 
**Flooding** increases soil water to water holding capacity and then adds water equivalent to the depth of flooding. Subsequent irrigation events maintain flooding by topping off water content.

 Flooding may also reduce the drainage parameter  $(f_{\text{drain}})$ close to zero \eq{eq:drainage}.

Flooded or high‑volume irrigation (not yet implemented) would be represented by large $F^W_{\text{irrig}}$ and (optionally) a modified drainage parameter $f_{\text{drain}}$.

\begin{equation}
F^W_{\text{irrigation}} = 
\begin{cases}
f_{\text{intercept}} \cdot F^W_{\text{irrigation}} & \text{canopy} \\
F^W_{\text{irrigation}} & \text{soil} \\
W_{\text{WHC}} - W_{\text{soil}} + F^W_{\text{irrigation}} & \text{flooding}
\end{cases}
\label{eq:irrigation}
\end{equation}

-->

## References

Braswell, Bobby H., William J. Sacks, Ernst Linder, and David S. Schimel. 2005. Estimating Diurnal to Annual Ecosystem Parameters by Synthesis of a Carbon Flux Model with Eddy Covariance Net Ecosystem Exchange Observations. Global Change Biology 11 (2): 335–55. https://doi.org/10.1111/j.1365-2486.2005.00897.x.

Gutschick, V.P., 1981. Evolved strategies in nitrogen acquisition by plants. Am. Nat. 118, 607–637. https://doi.org/10.1086/283858

Libohova, Z., Seybold, C., Wysocki, D., Wills, S., Schoeneberger, P., Williams, C., Lindbo, D., Stott, D. and Owens, P.R., 2018. Reevaluating the effects of soil organic matter and other properties on available water-holding capacity using the National Cooperative Soil Survey Characterization Database. Journal of soil and water conservation, 73(4), pp.411-421.

Manzoni, Stefano, and Amilcare Porporato. 2009. Soil Carbon and Nitrogen Mineralization: Theory and Models across Scales. Soil Biology and Biochemistry 41 (7): 1355–79. https://doi.org/10.1016/j.soilbio.2009.02.031.

Oleson, K.W., Lawrence, D.M., Bonan, G.B., Flanner, M.G., Kluzek, E., Lawrence, P.J., Levis, S., Swenson, S.C., Thornton, P.E., Dai, A. and Decker, M., 2010. Technical description of version 4.0 of the Community Land Model (CLM). National Center for Atmospheric Research, Boulder, CO, USA.

Parton, W. J., E. A. Holland, S. J. Del Grosso, M. D. Hartman, R. E. Martin, A. R. Mosier, D. S. Ojima, and D. S. Schimel. 2001. Generalized Model for NOx  and N2O Emissions from Soils. Journal of Geophysical Research: Atmospheres 106 (D15): 17403–19. https://doi.org/10.1029/2001JD900101.

Rastetter, E.B., Vitousek, P.M., Field, C., Shaver, G.R., Herbert, D., Gren, G.I., 2001. Resource optimization and symbiotic nitrogen fixation. Ecosystems 4, 369–388. https://doi.org/10.1007/s10021-001-0018-z

Wang H, Yan Z, Ju X, Song X, Zhang J, Li S and Zhu-Barker X (2023) Quantifying nitrous oxide production rates from nitrification and denitrification under various moisture conditions in agricultural soils: Laboratory study and literature synthesis. Front. Microbiol. 13:1110151. https://doi.org/10.3389/fmicb.2022.1110151

Zobitz, J. M., D. J. P. Moore, W. J. Sacks, R. K. Monson, D. R. Bowling, and D. S. Schimel. 2008. “Integration of Process-Based Soil Respiration Models with Whole-Ecosystem CO2 Measurements.” Ecosystems 11 (2): 250–69. https://doi.org/10.1007/s10021-007-9120-1.

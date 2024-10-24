# Model Structure

## Temperature and Moisture Dependence 

Rates of decomposition, nitrification, denitrification, and methanogenesis are depend on temperature and soil moisture.

### Temperature dependence $$f_{T}$$

Used for photosynthesis (eq A9). 

For initial model, we will use a single Q10 for all reactions. 

$$
f_{T}=\max\left(\frac{(T_{max} - T_{air})(T_{air} - T_{min})}{\left(\frac{(T_{max} - T_{min})}{2}\right)^2}, 0\right)
$$

$$
f_{temp} = Q_{10}^{(T - 20)/10}
$$

### Soil Moisture dependence

Might need to find a function with an optimal W$_S$ != 1.

$$
f_{W} = 1 - \frac{W_{S}}{W_{c}}
$$

### Dependence on soil anaerobic conditions

$$d_{anaer} = \frac{W_{S}}{W_{c}} \quad \text{if} \quad W_{S} > W_{c}$$

## Rates

### Fertilization (no3, nh4, om)

$$
R_{nh4_fert} = \textrm{provided as a driver}
$$

$$
R_{no3_fert} = \textrm{provided as a driver}
$$

$$
R_{org_fert} = \textrm{provided as a driver}
$$

### Decomposition (C$$_S$$ $\rightarrow$ CO$$_2$$)

$$
R_{dec} = K_{dec} \cdot f_{T} \cdot f_{W}
$$

### N Mineralization (N$$_S$$ $$\rightarrow$$ NH$$_4$$)

$$
R_{min} = R_{dec} \cdot N_{S} \cdot f_{T} \cdot f_{W}
$$


### Nitrification (NH$$_4$$ $$\rightarrow$$ NO$$_3$$ & N$$_2$$O )

$$
R_{nitr} = K_{nitr} \cdot NH_4 \cdot f_{T} \cdot f_{W}
$$

### Denitrification (NO$$_3$$ $$\rightarrow$$ N$$_2$$O )

$$
R_{denitr} = K_{denitr} \cdot NO_3 \cdot f_{T} \cdot f_{anaer}
$$

### Methane Production (C$$_S$$ $$\rightarrow$$ CH$$_4$$)

Need to modify this for to account for diffusion, ebullition, and plant transport

$$
R_{meth} = K_{meth} \cdot C_S \cdot f_{anaer}
$$

### Methane Oxidation (CH$$_4$$ $$\rightarrow$$ CO$$_2$$)

$$
R_{methox} = K_{methox} \cdot CH_4 \cdot f_{T} \cdot f_{W}
$$

### Nitrogen Fixation (addition of N$$_S$$??)

if a nitrogen fixing plant is present, N fixation is represented as a function of plant growth, needs to have a carbon cost to the plant 

$$
R_{fix} = K_{fix} \cdot R_{growth}
$$

## Differential Equations for Fluxes and Pools

### Soil Litter

TBD

### Soil Organic Carbon

$$
\frac{dC_S}{dt} = -R_{dec} \cdot C_S
$$

### Soil Organic Nitrogen

$$
\frac{dN_S}{dt} = -R_{dec} \cdot N_S + R_{fix} + R_{updake}
$$

### Soil Ammonium

$$
\frac{dNH_4}{dt} = R_{min} \cdot N_S + R_{nh4fert} - R_{nitr}
$$

### Soil Nitrate 

$$
\frac{dNO_3}{dt} = R_{nitr}  + R_{nh4fert} - R_{denitr}
$$

### Nitrous Oxide

$$
\frac{dN_2O}{dt} = f_{N2O_{nitr}} \cdot R_{nitr} + f_{N2O_{denitr}} \cdot R_{denitr}
$$

### Methane Flux

$$
\frac{dCH_4}{dt} = R_{meth} - R_{methox}
$$

## Internally calculated states

- $$d_{anaer}$$ Fraction of soil that is anaerobic.
- $$T_{soil}$$ soil temperature
- N_pool_i = C_pool_i / CN_pool_i

## References

Braswell, Bobby H., William J. Sacks, Ernst Linder, and David S. Schimel. 2005. Estimating Diurnal to Annual Ecosystem Parameters by Synthesis of a Carbon Flux Model with Eddy Covariance Net Ecosystem Exchange Observations. Global Change Biology 11 (2): 335–55. https://doi.org/10.1111/j.1365-2486.2005.00897.x.


Manzoni, Stefano, and Amilcare Porporato. 2009. Soil Carbon and Nitrogen Mineralization: Theory and Models across Scales. Soil Biology and Biochemistry 41 (7): 1355–79. https://doi.org/10.1016/j.soilbio.2009.02.031.

Parton, W. J., E. A. Holland, S. J. Del Grosso, M. D. Hartman, R. E. Martin, A. R. Mosier, D. S. Ojima, and D. S. Schimel. 2001. Generalized Model for NOx  and N2O Emissions from Soils. Journal of Geophysical Research: Atmospheres 106 (D15): 17403–19. https://doi.org/10.1029/2001JD900101.

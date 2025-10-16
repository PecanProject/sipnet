# Model Inputs

These are the input files needed to run SIPNET:
1. `sipnet.param`: Model parameter file.
2. `<sitename>.clim`: Climate file, provides weather data for each time step of the simulation period.
3. [optional] `events.in`: Agronomic events.
4. [optional] Run time options (command line arguments or config file `sipnet.in`)

## Parameters and Initial Conditions

Both initial conditions and parameters are specified in a file named `sipnet.param`.

The SIPNET parameter file (`sipnet.param`) specifies model parameters and their properties for each simulation. 
Each line in the file corresponds to a single parameter and contains five or six space-separated values.

| Column         | Description                                |
| -------------- | ------------------------------------------ |
| Parameter Name | Name of the parameter                      |
| Value          | Value of the parameter to use in the model |

### Example `sipnet.param` file

Column names are not used, but are:

```
param_name value
```

The first lines in `sipnet.param` could be:

```
plantWoodInit 110
laiInit 0
litterInit 200
soilInit 7000
litterWFracInit 0.5
soilWFracInit 0.6
snowInit 1
microbeInit 0.5
fineRootFrac 0.2
coarseRootFrac 0.2
aMax 95
aMaxFrac 0.85
...
```

## Climate

For each step of the model, the following inputs are needed. These are provided in a file named `<sitename>.clim` with the following columns:

| col | parameter | description                                                          | units                                                                          | notes                                                                                                          |
| --- | --------- | -------------------------------------------------------------------- | ------------------------------------------------------------------------------ | -------------------------------------------------------------------------------------------------------------- |
| 1   | year      | year of start of this timestep                                       |                                                                                | integer, e.g. 2010                                                                                             |
| 2   | day       | day of start of this timestep                                        | Day of year                                                                    | 1 = Jan 1                                                                                                      |
| 3   | time      | time of start of this timestep                                       | hours after midnight                                                           | e.g. noon = 12.0, midnight = 0.0, can be a fraction                                                            |
| 4   | length    | length of this timestep                                              | days                                                                           | variable-length timesteps allowed, typically not used                                                          |
| 5   | tair      | avg. air temp for this time step                                     | degrees Celsius                                                                |                                                                                                                |
| 6   | tsoil     | average soil temperature for this time step                          | degrees Celsius                                                                | can be estimated from Tair                                                                                     |
| 7   | par       | average photosynthetically active radiation (PAR) for this time step | $\text{Einsteins} \cdot m^{-2} \text{ground area} \cdot \text{time step}^{-1}$ | input is in Einsteins \* m^-2 ground area, summed over entire time step                                        |
| 8   | precip    | total precip. for this time step                                     | cm                                                                             | input is in mm; water equivilant - either rain or snow                                                         |
| 9   | vpd       | average vapor pressure deficit                                       | kPa                                                                            | input is in Pa, can be calculated from air temperature and relative humidity.                                  |
| 10  | vpdSoil   | average vapor pressure deficit between soil and air                  | kPa                                                                            | input is in Pa ; differs from vpd in that saturation vapor pressure is calculated using Tsoil rather than Tair |
| 11  | vPress    | average vapor pressure in canopy airspace                            | kPa                                                                            | input is in Pa                                                                                                 |
| 12  | wspd      | avg. wind speed                                                      | m/s                                                                            |                                                                                                                |

Note: An older format for this file included location as the first column and soilWetness as the last column. Files with this older format can still be read by sipnet:
* SIPNET will print a warning indicating that it is ignoring the obsolete columns
* If there is more than one location specified in the file, SIPNET will error and halt

### Example `sipnet.clim` file:

Column names are not used, but are:

```
loc	year day  time length tair tsoil par    precip vpd   vpdSoil vPress wspd
```

**Half-hour time step**

```
0	1998 305  0.00    -1800   1.9000   1.2719   0.0000   0.0000 109.5364  77.5454 726.6196   1.6300
0	1998 305  0.50    -1800   1.9000   1.1832   0.0000   0.0000 109.5364  73.1254 726.6196   1.6300
0	1998 305  1.00    -1800   2.0300   1.1171   0.0000   0.0000 110.4243  63.9567 732.5092   0.6800
0	1998 305  1.50    -1800   2.0300   1.0439   0.0000   0.0000 110.4243  60.3450 732.5092   0.6800
```

**Variable time step**

```
0	  1998 305  0.00  0.292 1.5  0.8   0.0000 0.0000 105.8 70.1    711.6  0.9200
0	  1998 305  7.00  0.417 3.6  1.8   5.6016 0.0000 125.7 23.5    809.4  1.1270
0	  1998 305 17.00  0.583 1.9  1.3   0.0000 0.0000 108.1 75.9    732.7  1.1350
0	  1998 306  7.00  0.417 2.2  1.4   2.7104 1.0000 114.1 71.6    741.8  0.9690
```

## Agronomic Events

Agronomic (management) events are read from an `events.in` file. This file specifies one event per line:

| col | parameter   | description                          | units  | notes                                            |
| --- | ----------- | ------------------------------------ | ------ | ------------------------------------------------ |
| 1   | year        | Year of the event                    |        | e.g. 2025                                        |
| 2   | day         | Day of year of the event             | DOY    | 1 = Jan 1                                        |
| 3   | event_type  | Event type code                      |        | one of: `plant`, `harv`, `till`, `fert`, `irrig` |
| 4…n | event_param | Type‑specific parameters (see below) | varies | Order depends on event type                      |

Rules:

- File must be in chronological order. Ties (same day multiple events) are allowed and processed in file order.
- Events are specified at day resolution (no sub‑daily timestamp).
- Every (year, day) appearing in `events.in` must have at least one corresponding climate record; otherwise SIPNET errors.
- Values in `events.in` are instantaneous amounts (cm or mass/area) applied on the date and time of the first climate record matching that event's year and day.

See subsections below for details and parameter definitions for each event type.

### Irrigation

| parameter |  col  | req?  | description                                 |
| --------- | :---: | :---: | ------------------------------------------- |
| amount    |   5   |   Y   | Amount added (cm)                           |
| method    |   6   |   Y   | 0=canopy<br>1=soil<br>2=flood (placeholder) |

Model representation: An irrigation event increases soil moisture. A fraction of canopy irrigation is immediately evaporated.

Specifically: 

- For `method=soil`, this amount of water is added directly to the `soilWater` state variable 
- For `method=canopy`, a fraction of the irrigation water (determined by input param `immedEvapFrac`) is added to the flux state variable `immedEvap`, with the remainder going to `soilWater`.
- Initial implementation assumes that LITTER_WATER is not on. This might be revisited at a later date.


### Fertilization

| parameter |  col  | req?  | description |
| --------- | :---: | :---: | ----------- |
| org-N     |   5   |   Y   | g N / m2    |
| org-C     |   6   |   Y   | g C / m2    |
| min-N     |   7   |   Y   | g N / m2    |
<!--(NH4+NO3 in one pool model; NH4 in two pool model)
| min-N2 |   8   | Y*          | g N / m2 (*not unused in one pool model, NO3 in two pool model) | 
-->

  - Model representation: increases size of mineral N and litter C and N. Urea-N is assumed to be mineral N.
<!-- or NH4 in two pool model ... common assumption (e.g. DayCent) unless urease inhibitors are represented.-->
  - The code that generates `events.in` will handle conversion from fertilizer amount and type to mass of N and C allocated to different pools. In PEcAn this is done by the `PEcAn.SIPNET::write.configs.SIPNET()` function.

### Tillage

| parameter                        |  col  | req?  | description         |
| -------------------------------- | :---: | :---: | ------------------- |
| tillageEff $(f_{\textrm{till}})$ |   5   |   Y   | Adjustment to $R_H$ |

- Model representation:
  - Transient increase in decomposition rate by $f_{\text{,tillage}}$ that exponentially decays over time.
  - Multiple tillage events are additive.

### Planting

| parameter     |  col  | req?  | description                                  |
| ------------- | :---: | :---: | -------------------------------------------- |
| leaf-C        |   5   |   Y   | C added to leaf pool (g C / m2)              |
| wood-C        |   6   |   Y   | C added to above-ground wood pool (g C / m2) |
| fine-root-C   |   7   |   Y   | C added to fine root pool (g C / m2)         |
| coarse-root-C |   8   |   Y   | C added to coarse root pool (g C / m2)       |

- Model representation:
  - Date of event is the date of emergence, not the date of actual planting 
  - Increases size of carbon pools by the amount of each respective parameter
  - $N$ pools are calculated from $CN$ stoichiometric ratios.
- notes: PFT (crop type) is not an input parameter for a planting event because SIPNET only represents a single PFT.

### Harvest

| parameter                                                  |  col  | req?  | description           |
| ---------------------------------------------------------- | :---: | :---: | --------------------- |
| fraction of aboveground biomass removed                    |   5   |   Y   |                       |
| fraction of belowground biomass removed                    |   6   |   N   | default = 0           |
| fraction of aboveground biomass transferred to litter pool |   7   |   N   | default = 1 - removed |
| fraction of belowground biomass transferred to litter pool |   8   |   N   | default = 1 - removed |

- model representation:
  - biomass C and N pools are either removed or added to litter
   - for annuals or plants terminated, no biomass remains (col 5 + col 7 = 1 and col 6 + col 8 = 1). 
  - for perennials, some biomass may remain (col 5 + col 7 <= 1 and col 6 + col 8 <= 1; remainder is living).
   - root biomass is only removed for root crops
 
### Example of `events.in` file:

```
2022  35  till   0.2      # tilled on day 35, f_till = 0.2 (20% boost to rate term)
2022  40  till   0.1      # tilled on day 40, adds 0.1 to f_till
2022  40  irrig  5 1      # 5cm canopy irrigation on day 40 applied to soil
2022  40  fert   0 0 10   # fertilized with 10 g / m2 N_min on day 40 of 2022
2022  50  plant  10 3 2 5 # plant emergence on day 50 with 10/3/2/4 g C / m2, respectively, added to the leaf/wood/fine root/coarse root pools 
2022  250 harv   0.1      # harvest 10% of aboveground plant biomass on day 250
```

## Run-time Options

Configuration settings are applied in the following order of precedence:

1. Default values built into SIPNET
2. Values from the configuration file
3. Command-line arguments

Thus, command-line arguments override settings in the configuration file, and configuration file settings override default values.

### Input / Output Options

| Option       | Default   | Description                           |
| ------------ | --------- | ------------------------------------- |
| `input-file` | sipnet.in | Name of input config file             |
| `file-name`  | sipnet    | Prefix of climate and parameter files |

### Output Flags

| Option              | Default | Description                                                    |
| ------------------- | ------- | -------------------------------------------------------------- |
| `do-main-output`    | on      | Print time series of all output variables to `<file-name>.out` |
| `do-single-outputs` | off     | Print outputs one variable per file (e.g. `<file-name>.NEE`)   |
| `dump-config`       | on      | Print final config to `<file-name>.config`                     |
| `print-header`      | on      | Whether to print header row in output files                    |
| `quiet`             | off     | Suppress info and warning message                              |

### Model Flags

| Option        | Default | Description                                                                              |
| ------------- | ------- | ---------------------------------------------------------------------------------------- |
| `events`      | on      | Enable event handling.                                                                   |
| `gdd`         | on      | Use growing degree days to determine leaf growth.                                        |
| `growth-resp` | off     | Explicitly model growth respiration, rather than including with maintenance respiration. |
| `leaf-water`  | off     | Calculate leaf pool and evaporate from that pool.                                        |
| `litter-pool` | off     | Enable litter pool in addition to single soil carbon pool.                               |
| `microbes`    | off     | Enable microbe modeling.                                                                 |
| `snow`        | on      | Keep track of snowpack, rather than assuming all precipitation is liquid.                |
| `soil-phenol` | off     | Use soil temperature to determine leaf growth.                                           |
| `water-hresp` | on      | Whether soil moisture affects heterotrophic respiration.                                 |

Note the following restrictions on these options:
 - `soil-phenol` and `gdd` may not both be turned on

### Command Line Arguments

Command-line arguments can be used to specify run-time options when starting SIPNET. The syntax is as follows:

```
sipnet [options]
```

Where `[options]` can include any of the run-time options listed above. 
Flags use the syntax `--flag` to turn them on, or `--no-flag` to turn them off. Other options are specified by `--option value`.

See `sipnet --help` for a full list of available command-line options.

### Configuration File Format

SIPNET reads a configuration file that specifies run-time options without using command-line arguments. By default, SIPNET looks for a file named `sipnet.in` in the current directory. These will be overwritten by command-line arguments if specified.

The configuration file uses a simple key-value format, `option = value`, 
with one option per line; comments follow `#`. Flags are specified as 0 for off and 1 for on.

#### Example Configuration File

Note that case is ignored for parameter names, as well as dashes and underscores.

```
# Base filename (used for derived filenames)
FILE_NAME = mysite

# Output options
DO_MAIN_OUTPUT = 1
DO_SINGLE_OUTPUTS = 0
DUMP_CONFIG = 1
PRINT_HEADER = 1
QUIET = 0

# Model options
EVENTS = 1
GDD = 1
GROWTH_RESP = 0
LEAF_WATER = 0
LITTER_POOL = 0
MICROBES = 0
SNOW = 1
SOIL_PHENOL = 0
WATER_HRESP = 1
```

When `DUMP_CONFIG` is on, SIPNET will output the final configuration (after applying all settings from defaults, configuration file, and command line) to a file named `<file-name>.config`.

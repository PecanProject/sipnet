# Running SIPNET

This guide covers how to run SIPNET with command-line options and configuration files.

## Quick Start

The basic command to run SIPNET is:

```bash
./sipnet
```

This will look for an input configuration file named `sipnet.in` in the current directory. If found, it reads settings from that file. If not found, it uses built-in defaults.

To use a different configuration file:

```bash
./sipnet --input-file myconfig.in
```

To use different input file prefixes (climate and parameter files):

```bash
./sipnet --file-name mysite
```

This will look for `mysite.clim` and `mysite.param` files.

## Command-Line Options

SIPNET accepts the following command-line options. All options are optional and override settings from the configuration file.

### Input/Output Options

| Option | Short | Argument | Default | Description |
| --- | --- | --- | --- | --- |
| `--input-file` | `-i` | `<filename>` | `sipnet.in` | Name of input configuration file |
| `--file-name` | `-f` | `<name>` | `sipnet` | Prefix for climate and parameter input files (looks for `<name>.clim` and `<name>.param`) |

### Model Feature Flags

These flags enable or disable optional model processes. Prepend `no-` to the flag name to disable it (e.g., `--no-events`).

| Flag | Default | Description |
| --- | --- | --- |
| `--events` | ON (1) | Enable agronomic event handling from `events.in` file |
| `--gdd` | ON (1) | Use growing degree days to determine when leaves grow |
| `--growth-resp` | OFF (0) | Explicitly model growth respiration separately from maintenance respiration |
| `--leaf-water` | OFF (0) | Calculate a separate leaf water pool and evaporate from it |
| `--litter-pool` | OFF (0) | Enable a separate litter pool in addition to the soil carbon pool |
| `--microbes` | OFF (0) | Enable microbe modeling for more detailed decomposition |
| `--nitrogen-cycle` | OFF (0) | Enable nitrogen cycle modeling (pool and flux tracking) |
| `--snow` | ON (1) | Track snowpack separately; if disabled, all precipitation is treated as liquid |
| `--soil-phenol` | OFF (0) | Use soil temperature (instead of growing degree days) to determine leaf growth |
| `--water-hresp` | ON (1) | Allow soil moisture to affect heterotrophic respiration rates |

#### Model Flag Restrictions

The following flag combinations are mutually exclusive:

- `--soil-phenol` and `--gdd` cannot both be enabled
- `--events` and `--microbes` cannot both be enabled
- `--nitrogen-cycle` and `--microbes` cannot both be enabled

### Output Flags

These flags control what outputs are generated. Prepend `no-` to disable (e.g., `--no-print-header`).

| Flag | Default | Description |
| --- | --- | --- |
| `--do-main-output` | ON (1) | Write time series of all output variables to `<file-name>.out` |
| `--do-single-outputs` | OFF (0) | Write one output variable per file (e.g., `<file-name>.NEE`, `<file-name>.GPP`) |
| `--dump-config` | OFF (0) | Write final merged configuration to `<file-name>.config` after running |
| `--print-header` | ON (1) | Print header row with variable names in output files |
| `--quiet` | OFF (0) | Suppress informational and warning messages to console |

### Information Options

| Option | Short | Description |
| --- | --- | --- |
| `--help` | `-h` | Print help message and exit |
| `--version` | `-v` | Print SIPNET version and exit |

## Configuration Files

SIPNET reads configuration from a file (default: `sipnet.in`). This file uses a simple key-value format with one setting per line.

### Configuration File Format

Each line in the configuration file contains a key-value pair separated by whitespace:

```
KEY VALUE
```

Keys are case-insensitive and can use hyphens or underscores (e.g., `EVENTS`, `events`, `EVENTS`, `events` are equivalent).

### Configuration File Keys

#### Input/Output Keys

| Key | Value Type | Description |
| --- | --- | --- |
| `INPUT_FILE` | string | Name of configuration file to read |
| `FILE_NAME` | string | Prefix for climate and parameter input files |
| `PARAM_FILE` | string | Path to model parameters file (optional; defaults to `<FILE_NAME>.param`) |
| `CLIM_FILE` | string | Path to climate file (optional; defaults to `<FILE_NAME>.clim`) |
| `OUT_FILE` | string | Path for main output file (optional; defaults to `<FILE_NAME>.out`) |
| `OUT_CONFIG_FILE` | string | Path for config dump file (optional; defaults to `<FILE_NAME>.config`) |

#### Model Feature Keys

| Key | Value (1/0) | Description |
| --- | --- | --- |
| `EVENTS` | 0 or 1 | Enable/disable event handling |
| `GDD` | 0 or 1 | Use growing degree days for leaf growth |
| `GROWTH_RESP` | 0 or 1 | Explicitly model growth respiration |
| `LEAF_WATER` | 0 or 1 | Track separate leaf water pool |
| `LITTER_POOL` | 0 or 1 | Enable separate litter pool |
| `MICROBES` | 0 or 1 | Enable microbe modeling |
| `NITROGEN_CYCLE` | 0 or 1 | Enable nitrogen cycle modeling |
| `SNOW` | 0 or 1 | Track snowpack |
| `SOIL_PHENOL` | 0 or 1 | Use soil temperature for phenology |
| `WATER_HRESP` | 0 or 1 | Allow soil moisture to affect respiration |

#### Output Keys

| Key | Value (1/0) | Description |
| --- | --- | --- |
| `DO_MAIN_OUTPUT` | 0 or 1 | Write combined output file |
| `DO_SINGLE_OUTPUT` | 0 or 1 | Write individual output files per variable |
| `DUMP_CONFIG` | 0 or 1 | Dump final configuration |
| `PRINT_HEADER` | 0 or 1 | Include header row in output files |
| `QUIET` | 0 or 1 | Suppress console messages |

### Example Configuration File

Here's an example `sipnet.in` configuration file:

```
# Input files
FILE_NAME my_site
PARAM_FILE ../inputs/my_site.param
CLIM_FILE  ../inputs/my_site.clim

# Model features
EVENTS 1
GDD 1
SNOW 1
NITROGEN_CYCLE 0
MICROBES 0

# Output
DO_MAIN_OUTPUT 1
DO_SINGLE_OUTPUT 0
PRINT_HEADER 1
QUIET 0
```

## Option Precedence

SIPNET applies configuration in this order (later values override earlier ones):

1. **Built-in Defaults** — hardcoded defaults for all options
2. **Configuration File** — settings from `sipnet.in` (or file specified by `--input-file`)
3. **Command-Line Arguments** — options passed to `./sipnet` on the command line

### Example: Precedence in Action

Given this configuration file (`sipnet.in`):

```
FILE_NAME base_site
EVENTS 1
LITTER_POOL 0
```

Running with command-line overrides:

```bash
./sipnet --file-name override_site --no-events
```

Results in:

| Option | Source | Value |
| --- | --- | --- |
| `file-name` | Command line | `override_site` |
| `events` | Command line | OFF (0) — `--no-events` overrides config file's `EVENTS 1` |
| `litter-pool` | Config file | OFF (0) — not overridden |

## Common Use Patterns

### 1. Run with Default Settings and Standard File Names

```bash
./sipnet
```

This uses:
- Configuration file: `sipnet.in` (if it exists)
- Input files: `sipnet.param`, `sipnet.clim`
- All defaults from the model

### 2. Run a Specific Site with Custom Config

```bash
./sipnet --input-file site_configs/oak_forest.in --file-name oak_forest
```

This reads configuration from `site_configs/oak_forest.in` and looks for input files `oak_forest.param` and `oak_forest.clim`.

### 3. Test Without Events

```bash
./sipnet --no-events
```

This disables agronomic events (management practices) even if `events.in` exists or events are enabled in the config file.

### 4. Enable Nitrogen Cycling

```bash
./sipnet --nitrogen-cycle
```

This enables nitrogen cycle tracking for the simulation. Note: `--nitrogen-cycle` and `--microbes` are mutually exclusive.

### 5. Generate Per-Variable Output Files

```bash
./sipnet --do-single-outputs
```

This creates separate output files for each variable (e.g., `sipnet.NEE`, `sipnet.GPP`, `sipnet.BIOMASS`) in addition to or instead of the main combined output.

### 6. Silent Run with Config Dump

```bash
./sipnet --quiet --dump-config
```

This runs without console output and saves the final merged configuration to `sipnet.config` for inspection and reproduction.

### 7. Compare Two Configurations

```bash
# Run base scenario
./sipnet --dump-config --file-name base > /dev/null

# Run alternative scenario  
./sipnet --input-file alt.in --file-name alt --dump-config > /dev/null

# Compare outputs
diff base.config alt.config
```

## Output Files

SIPNET generates output files based on your configuration:

### Main Output File

**Filename**: `<file-name>.out` (if `--do-main-output` is enabled, which is default)

This file contains time-series data with one row per time step and one column per output variable. If `--print-header` is enabled, the first row contains variable names.

Example output (first 5 rows):

```
GPP NEE BIOMASS LAI ...
123.45 45.23 1234.5 3.2 ...
125.12 46.01 1234.8 3.2 ...
128.34 47.15 1235.2 3.3 ...
...
```

### Per-Variable Output Files

**Filename pattern**: `<file-name>.<VARIABLE>`  (if `--do-single-outputs` is enabled)

Each variable is written to a separate file. Useful for working with specific model outputs (e.g., GPP for validation, LAI for remote sensing comparison).

### Configuration Dump File

**Filename**: `<file-name>.config` (if `--dump-config` is enabled)

Shows the final merged configuration after applying all defaults, config file, and command-line options. Useful for:
- Reproducing a run exactly
- Verifying that settings were applied correctly
- Archiving with results for full reproducibility

Example:

```
EVENTS 1
GDD 1
GROWTH_RESP 0
FILE_NAME my_site
PARAM_FILE my_site.param
CLIM_FILE my_site.clim
...
```

## Input Files Reference

For details on the format and contents of input files, see:

- **Model Parameters**: See the parameter reference in [Model States and Parameters](../parameters.md#sipnet-model-states-and-parameters)
- **Climate Data**: See [Climate](model-inputs.md#climate)
- **Model Input Files**: See [Model Inputs](model-inputs.md#model-inputs)

## Troubleshooting

### "Cannot open file: sipnet.in"

This message appears if:
- You're using a custom config file that doesn't exist
- The config file path is wrong

**Solution**: Check that the config file exists and the path is correct. If you don't need a config file, just don't provide `--input-file`; SIPNET will use defaults.

### Incompatible Flags Error

SIPNET will exit with an error if you try to enable mutually exclusive flags:
- `--soil-phenol` and `--gdd` together
- `--events` and `--microbes` together
- `--nitrogen-cycle` and `--microbes` together

**Solution**: Choose one flag from each incompatible pair.

### "Growing degree days not calculated for this site"

This warning occurs if `--gdd` is enabled but the climate file lacks necessary temperature data.

**Solution**: Ensure your climate file includes both air temperature (`tair`) and soil temperature (`tsoil`) columns.

## Advanced: Adding New CLI Options

To add new command-line options to SIPNET, see [Adding CLI Options](../developer-guide/cli-options.md#naming).

## See Also

- [Model Structure](../model-structure.md) — Overview of SIPNET's design and equations
- [Model Inputs](model-inputs.md) — Detailed input file formats
- [Model Outputs](model-outputs.md) — Output variables and descriptions

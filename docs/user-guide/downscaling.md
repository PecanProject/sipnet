# SIPNET Downscaling

SIPNET Downscaling combines a three-fold model ensemble with environmental
covariates to produce NEE and LE maps. The inference interface accepts an
original-grid window and period IDs, builds the four preceding predictor
windows, applies native three-hour and spatial corrections, and aggregates
supported predictions into period and monthly products.

## Installation and devices

Use Python 3.12 in a dedicated environment. From the repository root:

```bash
python -m venv /path/to/workspace/envs/downscaling
source /path/to/workspace/envs/downscaling/bin/activate
python -m pip install 'torch==2.12.1+cpu' --index-url https://download.pytorch.org/whl/cpu
python -m pip install -r downscaling/requirements.txt
```

For CUDA, install the CUDA build of PyTorch and the CUDA dependencies instead:

```bash
python -m pip install 'torch==2.12.1' --index-url https://download.pytorch.org/whl/cu130
python -m pip install -r downscaling/requirements-cuda.txt
```

CUDA requires a compatible NVIDIA driver and one visible GPU. Set
`CUDA_VISIBLE_DEVICES` to the allocated device and `resources.device: cuda`.
An unavailable requested device is an error; the program does not switch devices.
CPU workers prepare tiles while one process consumes the selected device.
Limit `workers` and `cpu_threads` to the current allocation; reserve one process
for model evaluation. Start with a small spatial window to establish memory and
storage requirements. No batch scheduler is required.

Supply a timezone table to use a Python-only environment. Otherwise install R
with the `lutz` package and ensure `Rscript` is on `PATH`; coordinate lookup uses
its accurate Olson timezone polygons. Configure Python and R temporary/cache
locations inside your writable workspace when working on a shared server.

## Configuration and inputs

Copy `downscaling/config/example.yaml` to a writable workspace and fill in the
paths. Relative paths resolve from the configuration file's directory.
`model_bundle`, all `sources` and `output` must be disjoint. The input locations
are read-only. Output must be a new directory for a new request.

The request contains `window: {row, col, height, width}` in zero-based original
grid coordinates and a list of `period_ids`. The grid must be a north-up
EPSG:4326 raster. The period calendar supplies the time interpretation;
arbitrary temporal cuts are not accepted. `resources.max_output_bytes` limits
run storage. Device and source identities are bound to the run. Scientific
feature order and preprocessing belong to the model bundle and cannot be
changed through YAML overrides.

| Source | Required representation |
| --- | --- |
| `grid` | Original reference GeoTIFF, defining pixel IDs and georeferencing |
| `periods` | CSV with unique integer `period_id`, `period_key`, UTC `start`, exclusive UTC `stop`; optional `hydrological_year`; includes four preceding windows |
| `era` | `ERA5_YEAR.nc`, classic NetCDF, ensemble fields `t2m`, `sp`, `d2m`, `tp`, `ssrd`, `strd`, `u10`, `v10` on time/member/latitude/longitude axes |
| `gldas` | Three-hour `GLDAS_NOAH025_3H.AYYYYMMDD.HH00.021.nc4.SUB.nc4` files with four soil-moisture layers in kg m-2 |
| `gldas_registry` | JSON schema `gldas-replacements-1`, with `replacements` containing UTC, replacement path and SHA-256; an empty list is valid |
| `annual_covariates` | `covariates_YEAR.tiff`; all years 2013–2024 define the union land domain, plus requested predictor years |
| `native_covariates` | `covariates_YEAR.tiff` for requested native calendar years |
| `native_lc` | Native land-cover raster; model-supported categories are 1–8 |
| `kgc`, `kgc_legend` | Climate-class raster and CSV mapping `code` to `KGC` |
| `aqua_catalog` | Complete MYD13Q1.061 catalog JSON with `start`, `stop`, `granules`; each granule has its original MODIS `title` |
| `aqua_receipts` | `TITLE.json` records containing `raw` and `layers`; each entry binds absolute `path`, byte `size`, and `mtime_ns`; layer filenames end in `_NDVI.tif` or `_EVI.tif` |
| `timezones` (optional) | CSV or Parquet with unique global pixel `index`, `lat`, `lon`, `tz`; null `tz` explicitly excludes that pixel |

Global pixel IDs are `row * original_grid_width + col`. The timezone table must
cover every land pixel in the request, including rows whose timezone is null.
Annual proxy bands are LC=1, disturbance age=2, biomass=3, pH=8, N=9, SOC=10,
sand=11 and elevation=15. Native static bands are disturbance age=2, biomass=3,
topographic wetness=4, pH=9, N=10, SOC=11 and sand=12.

ERA accumulations are read at the end of each `[t,t+3h)` window. Soil mass is
converted using the fixed layer-depth contract. Aqua packed values are scaled
once, and only bounded short gaps with same-sensor neighbors are filled.
Inputs must include support for the four lag windows and 64 days of vegetation
gap-fill context on either side. Missing declared files or receipts are errors.
A missing granule is accepted only when absent from the complete catalog.

## Portable model bundle

Obtain a bundle with this layout:

```text
manifest.json
proxy/fold_1.pt        # also folds 2 and 3
proxy/fold_1.json      # also folds 2 and 3
mean/fold_1_NEE.json   # both targets, all three folds
mean/fold_1_LE.json
spatial/fold_1.npz     # also folds 2 and 3
```

The manifest schema is `sipnet-downscaling-model-1`. It contains the exact
`contract` returned by `downscaling.models.bundle.feature_contract()`, a `files`
mapping of every relative artifact path to its SHA-256, provenance information,
and `id`, the `downscaling.artifacts.digest` of the manifest without `id`.
The loader validates all 15 artifacts before loading any model.

Proxy `.pt` files contain tensor state dictionaries. Their JSON metadata carries
`candidate`, `settings`, `preprocessing`, `target_center`, and `target_scale`.
The architecture has 67 numeric inputs and a 12-class land-cover embedding;
architecture settings come from the checkpoint metadata. Current model weights
use no year embedding. Numeric means, scales, feature order and target scales
must accompany the weights.

Mean-correction JSON files use XGBoost's saved-model format. Spatial NPZ files
contain `cell`, `caps`, `train_sites`, `site_ids`, `lc`, and `coords`. Site IDs
explicitly align the training subset, coordinates, classes and seasonal/hourly
residual cells. Runtime loading needs no training cache, scoring arrays or
external source-code tree. Original model provenance is retained separately
from the new bundle's content identity.

## Running and resuming

```bash
python -m downscaling.predict preflight --config /path/to/workspace/run.yaml
python -m downscaling.predict run --config /path/to/workspace/run.yaml
python -m downscaling.predict run --config /path/to/workspace/run.yaml --resume
python -m downscaling.predict report --config /path/to/workspace/run.yaml
```

Preflight checks the requested input inventory without creating output. A run
records its request, source ledger, model identity and inference code identity.
Prepared features and prediction blocks have completion receipts. Resume reuses
valid artifacts, checks all previously bound sources, and rejects changed
requests or stale identities. Use a new output directory after changing inputs,
models or inference code. A lock prevents concurrent writers to the same run.

A missing block, quality record, target or product cannot be treated as a
completed run. `report` verifies both targets and every requested period/month,
including grid, coverage and provenance, before writing `COMPLETE.json`.
An incomplete run returns a nonzero exit status with an explanation.

## Products

Products are cropped GeoTIFFs under `products/period/` and `products/month/`.
They retain the reference grid's resolution and the affine transform for the
requested window. Each file contains float32 bands:

| Band | Meaning |
| --- | --- |
| 1–3 | Supported mean for each fold |
| 4 | Mean across the three folds |
| 5 | Population standard deviation across folds; not a prediction interval |
| 6–8 | Valid native steps, expected steps, valid/expected fraction |
| 9 | Maximum spatial-neighbor distance in km across folds |
| 10 | Missing-reason bits: 1 proxy ineligible, 2 unsupported/missing native category, 4 missing timezone |
| 11 | Native steps using gap-filled vegetation |
| 12 | Longest consecutive unsupported native sequence among evaluated steps |

LE retains W m-2; NEE retains the supplied model's NEE convention, without a
new unit conversion. Water is NaN. Unsupported land has zero support and an
explicit reason; it is not represented as a zero flux. Timezone-excluded pixels
have no evaluated sequence: their gap-fill and longest-gap counters remain zero.
Native models preserve
missing continuous predictors according to their trained missing-value rules.

Monthly products aggregate **only requested windows**. Tags include
`coverage_windows_utc`, `expected_steps`, start and exclusive stop. Nonadjacent
requested windows remain separately listed, and missing runs are not joined
across uncovered gaps. A short request within a month is not a full-month mean.

## Validation

Fast synthetic tests run without acquired models or environmental data:

```bash
python -m pip install -r downscaling/requirements-test.txt
python -m pytest -q -p no:cacheprovider downscaling/tests
```

With a supplied reference directory and corresponding bundle, validate model
replay, actual feature preparation, native inference, GeoTIFFs and resume:

```bash
python -m downscaling.scripts.validate \
  --config /path/to/workspace/reference-run.yaml \
  --reference /path/to/reference
```

The reference manifest declares the exact request and hashes its expected files.
Use a reference captured on the same device for accumulated floating-point sums.
Use an empty output directory for the first run. The same command validates CPU
or CUDA according to the configuration. It checks exact IDs, shapes, masks,
float32 features and support counts, plus the established proxy, native and
spatial tolerances. `validation.json` records timings, memory and imported module
locations. Validation data and model binaries remain outside Git.

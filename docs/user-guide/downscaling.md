# SIPNET Downscaling

Models can be produced with the [training and calibration workflow](downscaling-training.md).

The ensemble predicts mean NEE and LE for a requested grid window and period.
Each fold combines a proxy prediction with a period residual forest and a monthly
spatial correction. Predictors include the current period and four previous windows.

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

| Input | Contents |
| --- | --- |
| `model_bundle` | Portable training export with three proxies, six residual forests, three monthly spatial models and a manifest of file hashes. |
| `grid`, `periods` | Original EPSG:4326 reference grid and CSV of integer period ID, period key, UTC start and exclusive stop. Include the four preceding windows. |
| `era` | Annual classic NetCDF ensemble files `ERA5_YEAR.nc` with t2m, sp, d2m, u10, v10, tp, ssrd and strd. Accumulations end at the next three-hour boundary. |
| `soil_cache` | One `gldas_NNN.npy` and its JSON metadata per period. Axes are three-hour time, latitude, longitude and four soil layers. Metadata identifies the array, sources, coordinates and UTC boundaries. |
| `aqua_ledger` | Dated MYD13Q1 tile inventory with original HDF, NDVI/EVI layer identities and acquisition receipts. Include 64 days around the predictor windows for short-gap interpolation. |
| `annual_covariates`, `native_covariates` | Annual proxy raster bands LC=1, disturbance age=2, biomass=3, pH=8, N=9, SOC=10, sand=11, elevation=15. Native bands are age=2, biomass=3, wetness=4, pH=9, N=10, SOC=11 and sand=12. Annual proxy rasters for 2013–2024 define the land union. |
| `native_lc`, `kgc`, `kgc_legend` | Native land-cover raster, climate-class raster and its code-to-KGC CSV. |
| `domain` | Optional directory with the land-union raster `domain.tif` and its annual-source manifest `domain.json`. Full-grid requests can also use its `modis_tiles.json` inventory. |
| `timezones` or `point_blocks` | Optional pixel timezone table (index, lat, lon, tz), or the shared directory of verified 256 × 256 point blocks. Without either source the run uses R and lutz. |
| `spatial_cache` | Optional shared production directory with the model manifest and precomputed monthly spatial corrections under `continental_cache/spatial/`. Omit for newly trained models. |

## Run and inspect products

```bash
python -m downscaling.predict preflight --config /path/to/run.yaml
python -m downscaling.predict run --config /path/to/run.yaml
python -m downscaling.predict run --resume --config /path/to/run.yaml
python -m downscaling.predict report --config /path/to/run.yaml
```

Group consecutive period IDs in one request to reuse the four preceding panels
in each block. Set the optional top-level `panel_cache` to the same writable
directory across requests to continue reuse between runs. Panels depend on the
period, pixel coordinates, covariate sources and sampling recipe. Changing the
model or device allows the same panels to be reused in a new output directory.
Resume verifies the frozen request and completed blocks.

Each period produces `products/period/NEE_NNN.tif` and `LE_NNN.tif`.
Bands 1–2 give the ensemble mean and population standard deviation; bands 3–5
give the individual folds. Band 6 is eligibility (1 or 0 on land). Band 7
records missing-reason bits: 1 for proxy/context, 2 for native land cover or
climate class, and 4 for timezone. Water is NoData. LE is in W/m² and NEE uses
the Yang target convention. Raster tags record UTC coverage and model identity.

To check a window against published grids on the same device, supply a directory
with reference files such as `p003/NEE_003.tif` and `p003/LE_003.tif`.

```bash
python -m downscaling.scripts.validate --config /path/to/run.yaml --reference /path/to/continental
```

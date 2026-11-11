# Training SIPNET Downscaling

Training consumes verified prepared arrays and produces a portable model bundle
for the [prediction interface](downscaling.md). Use the same dedicated Python
environment as prediction. CPU and CUDA execution are supported; select the
device explicitly. CUDA requires one visible GPU and the CUDA dependencies
described in the prediction installation instructions.

## Run training

Copy `downscaling/config/train.yaml` outside the repository and set the prepared
data directory, a new output directory, and resource limits:

```yaml
prepared_data: /path/to/prepared_training_data
output: /path/to/training_run
resources:
  device: cuda
  cpu_threads: 7
```

Run from the repository root:

```bash
python -m downscaling.train preflight --config /path/to/train.yaml
python -m downscaling.train run --config /path/to/train.yaml
python -m downscaling.train report --config /path/to/train.yaml
# Continue an interrupted run with its original data, code and configuration:
python -m downscaling.train run --config /path/to/train.yaml --resume
```

The fixed recipe uses three outer site folds, nested predictions for calibration,
and training-side preprocessing. The proxy uses a shared residual network with
12 land-cover categories, no year embedding, and a Student-t loss with four
degrees of freedom. Epoch selection uses up to 80 epochs and patience 10, followed
by a fresh fit on all fitting sites for the selected number of epochs. Mean and
interval correctors each use 200 boosting rounds. Spatial correction uses up to
20 same-land-cover neighbors and training-side clipping thresholds.

The configuration controls paths and resources. Scientific settings are recorded
by the recipe and cannot be overridden in the run YAML. A bounded validation
dataset reduces sites and temporal support while retaining the recipe. Full-size
training needs additional storage and compute; native matrices are streamed from
declared shards rather than concatenated into one in-memory matrix.

## Prepared data contract

The prepared directory contains `manifest.json`, `folds.csv`, `periods.csv`,
`panel/*.npy`, and `native/*`. The manifest uses schema
`sipnet-downscaling-training-data-1`, an explicit `expected_shards` list, a
`calendar_start_utc`, the model feature contract, and SHA-256/size records for
every required file. Its `id` is the digest of the remaining manifest fields.
`downscaling.train.data.publish` validates all arrays before publishing a manifest.
It preserves an identical existing manifest and rejects a changed release.

| Item | Meaning |
| --- | --- |
| `folds.csv` | Unique `index` site ID; `lat`, `lon`, `LC`, `is_tower`; `fold`, `inner_1` through `inner_3`, and nine `nested_<outer>_<inner>` columns. Nonparticipating inner/nested memberships are zero. |
| `periods.csv` | Unique `period_id` and `period_key`, UTC `start_utc`, inclusive `end_date`, and `expected_3hour_steps`. Rows follow chronological order. |
| `panel/site_ids.npy` | Explicit site order for all panel and native row indices. The folds table is aligned by ID. |
| `panel/numeric.npy` | Float32 `(period_count * site_count, 67)` in the declared proxy feature order; period-major layout. |
| `panel/target_mean.npy` | Float32 `(panel_rows, 2)`, the prepared SIPNET period means for NEE and LE in the prediction contract's units. |
| `panel/lc.npy`, `panel/years.npy` | Integer proxy land-cover codes 0–11 and calendar years for each panel row. |
| `panel/complete.npy` | Boolean eligibility mask; eligible rows require finite predictors and both targets. |
| `native/<id>.json` | The shard `name` and exact `rows` count. Empty declared shards are retained. |
| `native/<id>.x.npy` | Float32 `(sample, 33)`: 31 native features followed by zero-based KGC ID and LC code 1–8. Continuous missing values remain NaN. |
| `native/<id>.y.npy` | Float32 `(sample, 2)` prepared native targets for NEE and LE. |
| `native/<id>.row.npy` | Integer panel-row indices, consistent with site IDs, periods and eligibility. |
| `native/<id>.step.npy` | Integer three-hour offsets from `calendar_start_utc`. |
| `native/<id>.route.npy` | Float32 `(sample, 2, 3)` route fractions; all finite routes are retained. |

Native shards partition site/year pairs. Duplicate keys, overlapping partitions,
unknown categories, missing declared shards and changed files are errors. Site
subsets must preserve inner and nested membership from the full site inventory;
do not recompute these splits after selecting a subset.

For a receipt-verified prepared cache, the data-only import utility creates an
independent copy without importing the producing pipeline's code:

```bash
python -m downscaling.scripts.import_training_data \
  --source /path/to/prepared_cache_release \
  --source-manifest /path/to/prepared_cache_release/manifest.json \
  --shards /path/to/expected_shards.json \
  --output /path/to/new_prepared_training_data
```

`expected_shards.json` is an explicit list of source shard IDs. The source cache
must contain `cache/panel.json`, the panel arrays, folds/calendar tables and native
arrays, each with an original `.ok.json` receipt. The utility verifies every
declared source before copying. It never modifies source receipts. An optional
`--selection selection.json` declares `site_ids`, `period_ids` and a positive
`native_day_stride` for a bounded validation dataset; normal training uses the
complete prepared release.

## Outputs and recovery

`model_bundle/` contains three proxy models, six mean correctors and three spatial
models, with preprocessing and feature metadata. Set the prediction configuration's
`model_bundle` to this directory. The deployed loader does not read training
caches, interval models or historical scoring data.

The training run also preserves proxy checkpoints, interval correctors, nested
predictions, stage audits, final native out-of-fold predictions, and evaluation:

- `tables/metrics.csv`: native, period and monthly scores by target and fold,
  with native land-cover, route and neighbor-distance strata.
- `predictions/period_oof.h5` and `month_oof.h5`: truth means, predicted means,
  mean scales, actual support counts, site IDs and temporal keys.
- `REPORT.md`: evaluation overview. Aggregate scales use the shared-quantile
  assumption and are diagnostics rather than calibrated aggregate probabilities.
- `COMPLETE.json`: published only after all required stage products and the
  portable bundle have been verified.

A completed stage is skipped only when all its declared outputs and receipts
match the run. A completed stage with a missing or stale output fails explicitly;
it is not silently retrained. Incomplete proxy fits restart that fit, while
incomplete boosting fits can continue from their bound saved rounds. All completed
stages are preserved. Repeated preflight checks preserve the frozen manifest.
Changed inputs, scientific code or execution configuration require a new run.

## Validation

The standard CPU contract tests include splitting, preprocessing isolation,
checkpoint reload, both calibration objectives, spatial exclusion and recovery:

```bash
python -m pytest downscaling/tests
```

After completing a bounded training run, validate its exported bundle using a
raw proxy predictor fixture containing both supported and unsupported rows:

```bash
python -m downscaling.scripts.validate_training \
  --config /path/to/train.yaml \
  --proxy-fixture /path/to/proxy_predictors.parquet \
  --output /path/to/validation.json
```

The same command works for CPU or CUDA according to the configuration. It compares
proxy predictions, native correction and spatial lookup through the deployed
interfaces, verifies unchanged artifacts after resume, and records runtime module
sources and resource measurements. Retraining variability and fixed-checkpoint
prediction parity are separate checks.

## Optional historical comparison

Historical evaluation is an explicit command and is not needed for training,
completion or prediction:

```bash
python -m downscaling.train compare-history --config /path/to/train.yaml \
  --reference /path/to/historical_reference \
  --comparison-output /path/to/new_comparison
```

The reference uses schema `sipnet-downscaling-history-1`, a manifest `id` digest
and a `files` mapping of SHA-256 hashes. It contains `sites.csv` (`index`, `fold`),
`steps.csv` (unique ordered three-hour UTC `utc` values), and `thresholds.csv`
(one `cap` for each `fold`, `target`). Arrays `truth.npy`, `error.npy` and
`calibrated_scale.npy` use `(site, UTC step, target)` order;
`nearby_calendar_LC.npy` uses `(site, 96 month/hour bins, target)`;
`eligible.npy` is boolean `(site, UTC step)` and explicitly declares common-support
eligibility. All arrays follow the reference's own IDs. Missing or duplicate IDs
and thresholds are errors. Comparison uses matched supported native keys and each
product's own truth; a difference is not independent-observation skill gain.

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
by a fresh fit on all fitting sites for the selected number of epochs. Each target has a 200-tree residual forest on the 4,220-column period representation.
Training uses site-periods with complete three-hour target support, including
the actual lengths of the first and last windows. The target is the period
mean minus the corresponding cross-fitted proxy prediction. Only inner-fold
held-out errors contribute to the monthly spatial correction. Clipping uses
the support-weighted leave-self-out 99.9th percentile.

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
complete prepared release. Select complete periods with `native_day_stride: 1` for residual training.


## Outputs and replay

Pass `model_bundle/` to the prediction configuration. The training run also
retains held-out predictions, per-target MAE and RMSE in
`validation/period_scores.json`, checkpoint selection and stage receipts.
Re-read the exported forests and compare held-out predictions with:

```bash
python -m downscaling.scripts.validate_training --config /path/to/train.yaml
```

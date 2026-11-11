# SIPNET Downscaling

SIPNET Downscaling predicts NEE and LE from a portable model bundle and
meteorological, soil, vegetation and land-surface covariates. It produces
period and monthly GeoTIFFs on a requested window of the original grid.

From the repository root, use a dedicated Python 3.12 environment:

```bash
python -m venv /path/to/workspace/envs/downscaling
source /path/to/workspace/envs/downscaling/bin/activate
python -m pip install 'torch==2.12.1+cpu' --index-url https://download.pytorch.org/whl/cpu
python -m pip install -r downscaling/requirements.txt
cp downscaling/config/example.yaml /path/to/workspace/run.yaml
# Edit run.yaml to identify the model bundle, environmental inputs and output root.
python -m downscaling.predict preflight --config /path/to/workspace/run.yaml
python -m downscaling.predict run --config /path/to/workspace/run.yaml
python -m downscaling.predict report --config /path/to/workspace/run.yaml
```

Use `run --resume` to continue the same request. Inputs, code and model identities
must still match. Run data and model bundles belong outside this repository.
Importing `downscaling` performs no processing and creates no output.

See the [usage and input contract](../docs/user-guide/downscaling.md) for CUDA
installation, input formats, model packaging, output bands and validation commands.
The component has its own dependencies and is not part of the root Python
package installation.

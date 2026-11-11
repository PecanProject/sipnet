# SIPNET Downscaling

SIPNET Downscaling produces spatial estimates of net ecosystem exchange (NEE)
and latent heat flux (LE) from site-level SIPNET ensemble products and
environmental covariates.

The workflow combines model outputs with meteorological, vegetation and land
surface information to generate gridded flux products.

## Inputs

- Site coordinates and time-indexed SIPNET ensemble and target products.
- Environmental covariates, including weather, soil moisture and vegetation.
- Configuration specifying data, model and output locations.

## Outputs

- NEE and LE estimates aggregated over specified time windows.
- GeoTIFF maps with data coverage and quality information.
- Model and source metadata for reproducibility.

Large raster inputs are processed in spatial blocks, and completed work can be
reused when a run resumes.

"""Small explicit environmental fixtures; no acquired data required."""

import numpy as np
import pandas as pd
import pytest
import rasterio
from downscaling.configuration import Config
from downscaling.artifacts import write


@pytest.fixture
def run_case(tmp_path, monkeypatch):
    from downscaling.predict import state

    sources = {}
    inputs = tmp_path / "inputs"
    inputs.mkdir()
    for key in (
        "era",
        "gldas",
        "annual_covariates",
        "native_covariates",
        "aqua_receipts",
    ):
        sources[key] = inputs / key
        sources[key].mkdir()
    for key in (
        "grid",
        "periods",
        "gldas_registry",
        "native_lc",
        "kgc",
        "kgc_legend",
        "aqua_catalog",
    ):
        sources[key] = inputs / key
    raster_profile = dict(
        driver="GTiff",
        width=4,
        height=4,
        count=1,
        dtype="uint8",
        crs="EPSG:4326",
        transform=rasterio.Affine(0.1, 0, -90, 0, -0.1, 45),
    )
    for path in [
        sources["grid"],
        *[
            sources["annual_covariates"] / f"covariates_{y}.tiff"
            for y in range(2013, 2025)
        ],
    ]:
        with rasterio.open(path, "w", **raster_profile) as ds:
            ds.write(np.ones((1, 4, 4), np.uint8))
    for key in ("native_lc", "kgc", "kgc_legend"):
        sources[key].write_bytes(b"source")
    (sources["native_covariates"] / "covariates_2018.tiff").write_bytes(b"static")
    (sources["era"] / "ERA5_2018.nc").write_bytes(b"era")
    write(
        sources["gldas_registry"], dict(schema="gldas-replacements-1", replacements=[])
    )
    write(
        sources["aqua_catalog"],
        dict(start="2017-01-01", stop="2019-01-01", granules=[]),
    )
    dates = pd.date_range("2018-01-01", periods=6, tz="UTC")
    calendar = pd.DataFrame(
        dict(
            period_id=range(-3, 2),
            period_key=[str(d.date()) for d in dates[:-1]],
            start=dates[:-1],
            stop=dates[1:],
        )
    )
    calendar.to_csv(sources["periods"], index=False)
    for time in pd.date_range(dates[0], dates[-1], freq="3h", inclusive="left"):
        (
            sources["gldas"]
            / f"GLDAS_NOAH025_3H.A{time:%Y%m%d}.{time:%H}00.021.nc4.SUB.nc4"
        ).write_bytes(b"soil")
    config = Config(
        tmp_path / "models",
        tmp_path / "output",
        sources,
        dict(row=1, col=1, height=2, width=2),
        (1,),
        "cpu",
        1,
        1,
        10**7,
    )
    monkeypatch.setattr(state, "validate", lambda root: dict(id="model"))
    contract, needed, wanted = state.plan(config)
    return config, contract, needed, wanted

"""Boundary semantics of the actual environmental readers."""

import numpy as np
import pandas as pd
import pytest
import rasterio
from scipy.io import netcdf_file
from downscaling.prepare.met import MapEra
from downscaling.prepare.sampling import sample_many
from downscaling.prepare.sources import Sources
from downscaling.artifacts import write, identity


def test_window_sampler_nodata_and_bounds(tmp_path):
    path = tmp_path / "grid.tif"
    with rasterio.open(
        path,
        "w",
        driver="GTiff",
        width=2,
        height=2,
        count=2,
        dtype="float32",
        crs="EPSG:4326",
        transform=rasterio.transform.from_origin(0, 2, 1, 1),
        nodata=-999,
    ) as grid:
        grid.write(np.array([[[1, 2], [3, -999]], [[5, 6], [7, 8]]], np.float32))
    values = sample_many(path, [0.5, 1.5, 3], [1.5, 0.5, 1.5], [1, 2])
    np.testing.assert_array_equal(
        values, np.array([[1, 5], [np.nan, 8], [np.nan, np.nan]])
    )


def test_era_accumulation_crosses_year(tmp_path):
    for year, time, value in [(2019, "2019-12-31 21:00", 10), (2020, "2020-01-01", 20)]:
        with netcdf_file(tmp_path / f"ERA5_{year}.nc", "w") as source:
            for name, size in [
                ("time", 1),
                ("member", 2),
                ("latitude", 2),
                ("longitude", 2),
            ]:
                source.createDimension(name, size)
            for name, vals in [
                (
                    "time",
                    [
                        (
                            pd.Timestamp(time, tz="UTC")
                            - pd.Timestamp("1900-01-01", tz="UTC")
                        ).total_seconds()
                        / 3600
                    ],
                ),
                ("latitude", [1, 0]),
                ("longitude", [0, 1]),
            ]:
                source.createVariable(name, "d", (name,))[:] = vals
            for name in ("t2m", "sp", "d2m", "tp", "ssrd", "strd", "u10", "v10"):
                source.createVariable(
                    name, "f", ("time", "member", "latitude", "longitude")
                )[:] = value
    points = pd.DataFrame(dict(lat=[0.5, 5], lon=[0.5, 0.5]))
    with MapEra(tmp_path) as reader:
        result = reader.at_window(pd.Timestamp("2019-12-31 21:00", tz="UTC"), points)
        for name, value in result.items():
            assert value[0] == (20 if name in ("tp", "ssrd", "strd") else 10)
            assert np.isnan(value[1])
        with pytest.raises(ValueError, match="time absent"):
            reader.at_window(pd.Timestamp("2020-02-29", tz="UTC"), points)


def test_receipt_checks_original_and_layers(tmp_path):
    write(
        tmp_path / "registry.json", dict(schema="gldas-replacements-1", replacements=[])
    )
    write(
        tmp_path / "catalog.json",
        dict(start="2020-01-01", stop="2020-12-31", granules=[]),
    )
    source = Sources(
        dict(
            gldas_registry=tmp_path / "registry.json",
            aqua_catalog=tmp_path / "catalog.json",
            aqua_receipts=tmp_path,
        )
    )
    raw = tmp_path / "raw"
    layer = tmp_path / "layer"
    raw.write_bytes(b"raw")
    layer.write_bytes(b"layer")
    write(tmp_path / "granule.json", dict(raw=identity(raw), layers=[identity(layer)]))
    assert source.aqua_receipt(dict(title="granule"))
    raw.write_bytes(b"replaced")
    with pytest.raises(ValueError, match="Changed Aqua"):
        source.aqua_receipt(dict(title="granule"))


def test_timezone_exclusions_and_missing_table_ids(tmp_path):
    from types import SimpleNamespace
    from downscaling.prepare.domain import timezones

    points = pd.DataFrame(dict(index=[10, 11], lat=[45.0, 45.0], lon=[-90.0, -91.0]))
    table = points.copy()
    table["tz"] = None
    path = tmp_path / "timezones.parquet"
    table.to_parquet(path)
    config = SimpleNamespace(sources=dict(timezones=path))
    assert pd.isna(timezones(config, points)).all()
    table.iloc[:1].to_parquet(path)
    with pytest.raises(ValueError, match="Missing timezone table rows"):
        timezones(config, points)

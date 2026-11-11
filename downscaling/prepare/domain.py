"""Bounded original-grid land masks and explicit timezone exclusions."""

from pathlib import Path
import subprocess
import numpy as np
import pandas as pd
import rasterio
from rasterio.windows import Window
from zoneinfo import ZoneInfo
from downscaling.artifacts import digest


def tiles(config):
    w = config.window
    with rasterio.open(config.sources["grid"]) as grid:
        if (
            grid.crs != rasterio.crs.CRS.from_epsg(4326)
            or grid.transform.b
            or grid.transform.d
        ):
            raise ValueError("A north-up EPSG:4326 reference grid is required")
        if w["row"] + w["height"] > grid.height or w["col"] + w["width"] > grid.width:
            raise ValueError("Requested window exceeds reference grid")
    return [
        dict(
            row=row,
            col=col,
            height=min(256, w["row"] + w["height"] - row),
            width=min(256, w["col"] + w["width"] - col),
        )
        for row in range(w["row"], w["row"] + w["height"], 256)
        for col in range(w["col"], w["col"] + w["width"], 256)
    ]


def points_for_tile(config, sources, tile):
    window = Window(tile["col"], tile["row"], tile["width"], tile["height"])
    mask = np.zeros((tile["height"], tile["width"]), bool)
    with rasterio.open(config.sources["grid"]) as reference:
        for year in range(2013, 2025):
            with rasterio.open(sources.annual(year)) as source:
                if (source.shape, source.crs, source.transform) != (
                    reference.shape,
                    reference.crs,
                    reference.transform,
                ):
                    raise ValueError("Annual/reference grid mismatch")
                values = source.read(1, window=window, masked=True)
                if not np.isin(values.compressed(), np.arange(12)).all():
                    raise ValueError("Unknown annual land-cover code")
                mask |= (~np.ma.getmaskarray(values)) & (values.filled(0) > 0)
        rows, cols = np.nonzero(mask)
        rows += tile["row"]
        cols += tile["col"]
        lon, lat = rasterio.transform.xy(reference.transform, rows, cols)
        points = pd.DataFrame(
            dict(index=rows * reference.width + cols, lat=lat, lon=lon)
        )
    return points, mask


def timezones(config, points):
    if points.empty:
        return np.array([], dtype=object)
    table = config.sources.get("timezones")
    if table is not None:
        frame = (
            pd.read_parquet(table) if table.suffix == ".parquet" else pd.read_csv(table)
        )
        if frame["index"].duplicated().any():
            raise ValueError("Duplicate timezone point IDs")
        indexed = frame.set_index("index")
        missing = set(points["index"]) - set(indexed.index)
        if missing:
            raise ValueError(f"Missing timezone table rows: {len(missing)}")
        selected = indexed.loc[points["index"]]
        np.testing.assert_allclose(
            selected[["lat", "lon"]], points[["lat", "lon"]], rtol=0, atol=1e-10
        )
        zones = selected.tz.to_numpy()
    else:
        key = digest(points.to_dict(orient="list"))
        input_path = config.destination(f"tmp/timezones_{key}.input.csv")
        output_path = config.destination(f"tmp/timezones_{key}.csv")
        input_path.parent.mkdir(parents=True, exist_ok=True)
        points.to_csv(input_path, index=False)
        subprocess.run(
            [
                "Rscript",
                "--vanilla",
                str(Path(__file__).with_name("timezones.R")),
                str(input_path),
                str(output_path),
            ],
            check=True,
            capture_output=True,
            text=True,
        )
        frame = pd.read_csv(output_path)
        np.testing.assert_array_equal(frame["index"], points["index"])
        zones = frame.tz.to_numpy()
    for zone in pd.unique(zones):
        if pd.notna(zone):
            ZoneInfo(zone)
    return zones

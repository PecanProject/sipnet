"""Explicit environmental locations and verified acquired representations."""

from pathlib import Path
import re
import numpy as np
import pandas as pd
from rasterio.warp import transform
from downscaling.artifacts import read, identity, sha

GLDAS_FIELDS = [
    "SoilMoi0_10cm_inst",
    "SoilMoi10_40cm_inst",
    "SoilMoi40_100cm_inst",
    "SoilMoi100_200cm_inst",
]


class Sources:
    def __init__(self, paths):
        self.paths = paths
        registry = read(paths["gldas_registry"])
        if registry.get("schema") != "gldas-replacements-1":
            raise ValueError("Unknown soil replacement registry")
        self.replacements = {}
        for row in registry["replacements"]:
            time = pd.Timestamp(row["utc"])
            if time.tzinfo is None or time in self.replacements:
                raise ValueError("Invalid replacement time")
            path = Path(row["replacement_path"])
            if not path.is_absolute():
                path = paths["gldas_registry"].parent / path
            self.replacements[time] = (path.resolve(), row["sha256"])
        self.catalog = read(paths["aqua_catalog"])
        self.aqua = {}
        for row in self.catalog["granules"]:
            match = re.search(r"\.A(\d{4})(\d{3})\.(h\d{2}v\d{2})\.", row["title"])
            if match is None:
                raise ValueError("Unrecognized Aqua granule name")
            date = pd.Timestamp(f"{match[1]}-01-01", tz="UTC") + pd.Timedelta(
                days=int(match[2]) - 1
            )
            key = (date, match[3])
            if key in self.aqua:
                raise ValueError("Duplicate Aqua date/tile")
            self.aqua[key] = row

    def soil_path(self, time):
        time = pd.Timestamp(time)
        if time in self.replacements:
            return self.replacements[time][0]
        return (
            self.paths["gldas"]
            / f"GLDAS_NOAH025_3H.A{time:%Y%m%d}.{time:%H}00.021.nc4.SUB.nc4"
        )

    def soil_identity(self, time):
        path = self.soil_path(time)
        if time in self.replacements and sha(path) != self.replacements[time][1]:
            raise ValueError(f"Soil replacement hash mismatch: {path}")
        return identity(path)

    def annual(self, year):
        return self.paths["annual_covariates"] / f"covariates_{year}.tiff"

    def native(self, year):
        return self.paths["native_covariates"] / f"covariates_{year}.tiff"

    def aqua_receipt(self, row):
        path = self.paths["aqua_receipts"] / f'{row["title"]}.json'
        if not path.exists():
            raise ValueError(f"Missing Aqua receipt: {path}")
        record = read(path)
        for entry in [record["raw"], *record["layers"]]:
            expected = {key: entry[key] for key in ("path", "size", "mtime_ns")}
            if identity(entry["path"]) != expected:
                raise ValueError(f'Changed Aqua source: {entry["path"]}')
        return path, record

    def aqua_requirements(self, points, start, stop):
        dates = vi_calendar(start - pd.Timedelta(days=64), stop + pd.Timedelta(days=64))
        dates = dates[
            (dates >= start - pd.Timedelta(days=64))
            & (dates <= stop + pd.Timedelta(days=64))
        ]
        if (
            pd.Timestamp(self.catalog["start"], tz="UTC") > dates.min()
            or pd.Timestamp(self.catalog["stop"], tz="UTC") < dates.max()
        ):
            raise ValueError("Aqua catalog does not cover gap-fill support")
        tiles = aqua_tiles(points)
        return (
            dates,
            tiles,
            [
                self.aqua[date, tile]
                for date in dates
                for tile in np.unique(tiles)
                if (date, tile) in self.aqua
            ],
        )


def vi_calendar(start, stop):
    dates = []
    for year in range(pd.Timestamp(start).year, pd.Timestamp(stop).year + 1):
        dates.extend(
            pd.date_range(
                pd.Timestamp(f"{year}-01-09", tz="UTC"),
                pd.Timestamp(f"{year}-12-31", tz="UTC"),
                freq="16D",
            )
        )
    return pd.DatetimeIndex(dates)


def aqua_tiles(points):
    projection = "+proj=sinu +R=6371007.181 +nadgrids=@null +wktext"
    x, y = transform("EPSG:4326", projection, points.lon.tolist(), points.lat.tolist())
    span = 1111950.5196666666
    horizontal = np.floor((np.array(x) + 20015109.354) / span).astype(int)
    vertical = np.floor((10007554.677 - np.array(y)) / span).astype(int)
    return np.array([f"h{h:02d}v{v:02d}" for h, v in zip(horizontal, vertical)])

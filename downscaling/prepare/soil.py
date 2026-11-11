"""Read coarse soil layers once per period, then share memory-mapped windows."""

from pathlib import Path
import os
import numpy as np
import pandas as pd
import h5py
import rasterio
from rasterio.windows import Window, bounds
from downscaling.artifacts import identity, read, write, receipt, valid, require
from downscaling.prepare.sources import GLDAS_FIELDS


def cache_paths(config, period_id):
    base = config.destination(f"coarse/soil_{int(period_id)}")
    return base.with_suffix(".npy"), base.with_suffix(".json")


def _text(value):
    return value.decode() if isinstance(value, bytes) else str(value)


def prepare(config, sources, period, token):
    dest, metadata = cache_paths(config, period.period_id)
    times = pd.date_range(period.start, period.stop, freq="3h", inclusive="left")
    identities = [sources.soil_identity(time) for time in times]
    if metadata.exists():
        require(metadata, token)
        require(dest, token)
        record = read(metadata)
        if record["sources"] != identities or record["artifact"] != identity(dest):
            raise ValueError("Changed soil cache dependency")
        return record
    with h5py.File(identities[0]["path"]) as source:
        full_lat = source["lat"][:]
        full_lon = source["lon"][:]
    # Preserve the original domain and nearest-cell convention while cropping IO.
    rows = np.flatnonzero((full_lat >= 6.625) & (full_lat <= 85.375))
    cols = np.flatnonzero((full_lon >= -179.375) & (full_lon <= -19.625))
    if not len(rows) or not len(cols):
        raise ValueError("Soil source does not cover the model domain")
    original_lat = full_lat[rows]
    original_lon = full_lon[cols]
    window = config.window
    with rasterio.open(config.sources["grid"]) as grid:
        left, bottom, right, top = bounds(
            Window(window["col"], window["row"], window["width"], window["height"]),
            grid.transform,
        )
    rr = np.abs(original_lat[:, None] - np.array([bottom, top])).argmin(0)
    cc = np.abs(original_lon[:, None] - np.array([left, right])).argmin(0)
    rows = rows[max(0, int(rr.min()) - 1) : min(len(rows), int(rr.max()) + 2)]
    cols = cols[max(0, int(cc.min()) - 1) : min(len(cols), int(cc.max()) + 2)]
    rlo, rhi = int(rows.min()), int(rows.max()) + 1
    clo, chi = int(cols.min()), int(cols.max()) + 1
    lat, lon = full_lat[rlo:rhi], full_lon[clo:chi]
    shape = (len(times), len(lat), len(lon), 4)
    if np.prod(shape) * 4 > config.max_output_bytes:
        raise ValueError("Soil cache exceeds configured storage budget")
    dest.parent.mkdir(parents=True, exist_ok=True)
    partial = dest.with_name(dest.name + f".partial.{os.getpid()}")
    array = np.lib.format.open_memmap(partial, mode="w+", dtype=np.float32, shape=shape)
    for i, (time, item) in enumerate(zip(times, identities)):
        with h5py.File(item["path"]) as source:
            np.testing.assert_array_equal(source["lat"][:], full_lat)
            np.testing.assert_array_equal(source["lon"][:], full_lon)
            units = _text(source["time"].attrs["units"])
            encoded = pd.Timestamp(
                units.split(" since ")[1], tz="UTC"
            ) + pd.to_timedelta(float(source["time"][0]), unit="min")
            if encoded != time:
                raise ValueError(f'Soil timestamp mismatch: {item["path"]}')
            for j, name in enumerate(GLDAS_FIELDS):
                dataset = source[name]
                if _text(dataset.attrs["units"]) != "kg m-2":
                    raise ValueError("Changed soil units")
                values = dataset[0, rlo:rhi, clo:chi].astype(np.float32)
                values[
                    values
                    == float(np.asarray(dataset.attrs.get("_FillValue", -9999)).flat[0])
                ] = np.nan
                array[i, :, :, j] = values
    array.flush()
    del array
    partial.replace(dest)
    receipt(dest, token)
    record = dict(
        sources=identities,
        artifact=identity(dest),
        lat=lat.tolist(),
        lon=lon.tolist(),
        bounds=[
            float(original_lat.min()),
            float(original_lat.max()),
            float(original_lon.min()),
            float(original_lon.max()),
        ],
        start=str(period.start),
        stop=str(period.stop),
    )
    write(metadata, record)
    receipt(metadata, token)
    return record


class Reader:
    def __init__(self, config, points, periods, token):
        self.config = config
        self.points = points
        self.periods = periods
        self.token = token
        self.key = None

    def at(self, time):
        matches = self.periods[
            (self.periods.start <= time) & (self.periods.stop > time)
        ]
        if len(matches) != 1:
            raise ValueError("No unique soil period")
        period = next(matches.itertuples())
        key = int(period.period_id)
        if self.key != key:
            path, metadata = cache_paths(self.config, key)
            require(metadata, self.token)
            require(path, self.token)
            record = read(metadata)
            if record["artifact"] != identity(path):
                raise ValueError("Damaged soil cache")
            self.array = np.load(path, mmap_mode="r")
            lat = np.array(record["lat"])
            lon = np.array(record["lon"])
            self.rows = np.abs(lat[:, None] - self.points.lat.to_numpy()).argmin(0)
            self.cols = np.abs(lon[:, None] - self.points.lon.to_numpy()).argmin(0)
            self.lat = lat[self.rows]
            self.lon = lon[self.cols]
            ymin, ymax, xmin, xmax = record["bounds"]
            self.inside = (
                self.points.lat.between(ymin - 0.125, ymax + 0.125)
                & self.points.lon.between(xmin - 0.125, xmax + 0.125)
            ).to_numpy()
            self.key = key
        index = int((time - period.start).total_seconds() / 10800)
        values = self.array[index, self.rows, self.cols].astype(float)
        values[~self.inside] = np.nan
        return values, self.lat, self.lon

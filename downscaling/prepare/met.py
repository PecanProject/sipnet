"""Windowed ERA ensemble averaging and explicit accumulation end times."""

import numpy as np
import pandas as pd
import warnings
from scipy.io import netcdf_file


def bracket(axis, values):
    a = np.asarray(axis)
    order = np.argsort(a)
    s = a[order]
    v = np.asarray(values)
    hi = np.searchsorted(s, v, side="right").clip(1, len(s) - 1)
    lo = hi - 1
    w = (v - s[lo]) / (s[hi] - s[lo])
    inside = (v >= s[0]) & (v <= s[-1])
    return order[lo], order[hi], w, inside


class MapEra:
    def __init__(self, root):
        self.root = root
        self.files = {}
        self.maps = {}
        self.coords = None

    def close(self):
        for f in self.files.values():
            f.close()
        self.files = {}

    def __enter__(self):
        return self

    def __exit__(self, *args):
        self.close()

    def at_window(self, t, points):
        return {
            n: self.at(
                n,
                t + pd.Timedelta(hours=3) if n in ("tp", "ssrd", "strd") else t,
                points,
            )
            for n in ("t2m", "sp", "d2m", "tp", "ssrd", "strd", "u10", "v10")
        }

    def at(self, name, t, points):
        coords = points[["lat", "lon"]].to_numpy()
        if self.coords is None or not np.array_equal(
            coords, self.coords, equal_nan=True
        ):
            self.maps = {}
            self.coords = coords.copy()
        if t.year not in self.files:
            self.files[t.year] = netcdf_file(
                self.root / f"ERA5_{t.year}.nc", "r", mmap=True
            )
        f = self.files[t.year]
        encoded = (t - pd.Timestamp("1900-01-01", tz="UTC")).total_seconds() / 3600
        pos = np.flatnonzero(f.variables["time"][:] == encoded)
        if len(pos) != 1:
            raise ValueError(f"ERA time absent/duplicate: {t}")
        if t.year not in self.maps:
            r0, r1, wy, iy = bracket(f.variables["latitude"][:], points.lat)
            c0, c1, wx, ix = bracket(f.variables["longitude"][:], points.lon)
            rlo, rhi = int(min(r0.min(), r1.min())), int(max(r0.max(), r1.max())) + 1
            clo, chi = int(min(c0.min(), c1.min())), int(max(c0.max(), c1.max())) + 1
            self.maps[t.year] = (r0, r1, wy, iy, c0, c1, wx, ix, rlo, rhi, clo, chi)
        r0, r1, wy, iy, c0, c1, wx, ix, rlo, rhi, clo, chi = self.maps[t.year]
        v = f.variables[name]
        raw = v.data[int(pos[0]), :, rlo:rhi, clo:chi].astype(float)
        raw[raw == getattr(v, "_FillValue", -32767)] = np.nan
        raw = raw * getattr(v, "scale_factor", 1) + getattr(v, "add_offset", 0)
        with warnings.catch_warnings():
            warnings.simplefilter("ignore", RuntimeWarning)
            z = np.nanmean(raw, axis=0)
        out = np.zeros(len(points))
        total = np.zeros(len(points))
        for rr, cc, w in [
            (r0, c0, (1 - wy) * (1 - wx)),
            (r0, c1, (1 - wy) * wx),
            (r1, c0, wy * (1 - wx)),
            (r1, c1, wy * wx),
        ]:
            a = z[rr - rlo, cc - clo]
            good = np.isfinite(a) & (w > 1e-12)
            out[good] += a[good] * w[good]
            total[good] += w[good]
        result = np.divide(
            out, total, out=np.full(len(points), np.nan), where=total > 0
        )
        result[~(ix & iy)] = np.nan
        return result

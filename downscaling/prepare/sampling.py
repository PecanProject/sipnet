"""Windowed nearest native-pixel sampling equivalent to rasterio.sample."""

import numpy as np
import rasterio
from rasterio.warp import transform
from rasterio.windows import Window


def sample_many(path, lon, lat, bands):
    with rasterio.open(path) as src:
        x, y = transform("EPSG:4326", src.crs, list(lon), list(lat))
        x = np.asarray(x)
        y = np.asarray(y)
        rows, cols = rasterio.transform.rowcol(src.transform, x, y)
        rows = np.asarray(rows)
        cols = np.asarray(cols)
        valid = (
            (src.bounds.left <= x)
            & (x < src.bounds.right)
            & (src.bounds.bottom < y)
            & (y <= src.bounds.top)
            & (rows >= 0)
            & (rows < src.height)
            & (cols >= 0)
            & (cols < src.width)
        )
        out = np.full((len(x), len(bands)), np.nan)
        if valid.any():
            rr = rows[valid]
            cc = cols[valid]
            r0, r1 = int(rr.min()), int(rr.max()) + 1
            c0, c1 = int(cc.min()), int(cc.max()) + 1
            if (r1 - r0) * (c1 - c0) > max(1000000, len(rr) * 128):
                out[valid] = [
                    np.ma.asarray(v, dtype=float).filled(np.nan)
                    for v in src.sample(
                        zip(x[valid], y[valid]), indexes=bands, masked=True
                    )
                ]
            else:
                block = src.read(
                    bands, window=Window(c0, r0, c1 - c0, r1 - r0), masked=True
                )
                a = block[:, rr - r0, cc - c0].astype(float).filled(np.nan)
                out[valid] = a.T
        return out


def sample(path, lon, lat, band=1):
    return sample_many(path, lon, lat, [band])[:, 0]

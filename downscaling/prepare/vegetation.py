"""Native Aqua nearest sampling and bounded same-sensor gap filling."""

import numpy as np
import pandas as pd
from downscaling.contracts import short_gaps
from downscaling.prepare.sampling import sample


class Vegetation:
    def __init__(self, points, start, stop, sources):
        self.dates, tiles, _ = sources.aqua_requirements(points, start, stop)
        self.values = {}
        self.filled = {}
        # Validate each representation once, then sample both vegetation variables.
        receipts = {}
        for date in self.dates:
            for tile in np.unique(tiles):
                row = sources.aqua.get((date, tile))
                if row is not None:
                    receipts[date, tile] = sources.aqua_receipt(row)[1]
        for variable in ("EVI", "NDVI"):
            values = np.full((len(self.dates), len(points)), np.nan)
            for i, date in enumerate(self.dates):
                for tile in np.unique(tiles):
                    record = receipts.get((date, tile))
                    if record is None:
                        continue  # The complete source catalog declares no granule here.
                    matches = [
                        entry
                        for entry in record["layers"]
                        if entry["path"].endswith(f"_{variable}.tif")
                    ]
                    if len(matches) != 1:
                        raise ValueError(f"Missing/duplicate {variable} representation")
                    ix = np.flatnonzero(tiles == tile)
                    value = sample(
                        matches[0]["path"],
                        points.lon.to_numpy()[ix],
                        points.lat.to_numpy()[ix],
                    )
                    value[(value < -2000) | (value > 10000)] = np.nan
                    values[i, ix] = value * 0.0001
            flags = np.zeros_like(values, np.uint8)
            for j in range(len(points)):
                values[:, j], flags[:, j] = short_gaps(
                    values[:, j], self.dates, np.repeat("Aqua", len(values))
                )
            self.values[variable] = values
            self.filled[variable] = flags

    def at(self, time):
        distance = abs(self.dates - time.normalize())
        index = int(np.argmin(distance))
        if distance[index] > pd.Timedelta(days=9):
            raise ValueError("VI outside matching tolerance")
        return {
            **{name: a[index] for name, a in self.values.items()},
            **{name + "_filled": a[index] for name, a in self.filled.items()},
            "vi_date": self.dates[index],
            "vi_sensor": "Aqua",
        }

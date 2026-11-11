"""Streaming feature calculations, with explicit inputs and stable scientific semantics."""

import numpy as np
import pandas as pd
from zoneinfo import ZoneInfo
from downscaling.contracts import SOIL, DYNAMIC, rootzone, ppfd, unique
from downscaling.prepare.met import MapEra
from downscaling.prepare.sampling import sample as sample_raster, sample_many


class FeatureStream:
    def __init__(self, points, vegetation, soil, sources):
        unique(points, ["index"])
        self.sources = sources
        self.points = points.reset_index(drop=True)
        self.vi = vegetation
        self.era = MapEra(sources.paths["era"])
        self.static = {}
        self.proxy_static = {}
        self.soil = soil
        if self.points.tz.isna().any():
            raise ValueError(
                "Unresolved timezones must be excluded with a reason before feature evaluation"
            )

    def close(self):
        self.era.close()

    def at(self, t):
        p = self.points
        d = p[["index", "lon", "lat"]].copy()
        d["utc"] = t
        for k, v in self.era.at_window(t, p).items():
            d[k] = v
        a, lat, lon = self.soil.at(t)
        d[SOIL] = a
        d["soil_source_lat"] = lat
        d["soil_source_lon"] = lon
        d["soil_valid_time"] = t
        d["era_accum_end"] = t + pd.Timedelta(hours=3)
        if t.year not in self.static:
            self.static[t.year] = native_static(p, t.year, self.sources)
        for k, v in self.static[t.year].items():
            d[k] = v
        for k, v in self.vi.at(t).items():
            d[k] = v
        phase = np.empty(len(p))
        for zone in p.tz.unique():
            local = t.tz_convert(ZoneInfo(zone))
            value = (local.dayofyear + (local.hour + local.minute / 60) / 24) / (
                366 if local.is_leap_year else 365
            )
            phase[p.tz.to_numpy() == zone] = value
        d["sin_doy"] = np.sin(2 * np.pi * phase)
        d["cos_doy"] = np.cos(2 * np.pi * phase)
        d["PPFD"] = ppfd(d.ssrd)
        d["WindSpeed"] = np.hypot(d.u10, d.v10)
        tc = d.t2m - 273.15
        dc = d.d2m - 273.15
        d["VPD"] = np.maximum(
            0.6108 * np.exp(17.27 * tc / (tc + 237.3))
            - 0.6108 * np.exp(17.27 * dc / (dc + 237.3)),
            0,
        )
        d["is_day"] = np.where(np.isfinite(d.ssrd), (d.ssrd > 10).astype(float), np.nan)
        for k, v in {
            "t2m2": d.t2m**2,
            "PPFD2": d.PPFD**2,
            "EVI2": d.EVI**2,
            "VPD_t2m": d.VPD * d.t2m,
            "sincos": d.sin_doy * d.cos_doy,
        }.items():
            d[k] = v
        return d

    def period(self, row):
        times = pd.date_range(row.start, row.stop, freq="3h", inclusive="left")
        n = len(self.points)
        sums = np.zeros((n, 10))
        counts = np.zeros((n, 10), np.int32)
        for t in times:
            d = self.era.at_window(t, self.points)
            soil, _, _ = self.soil.at(t)
            tc = d["t2m"] - 273.15
            dc = d["d2m"] - 273.15
            es = 0.6108 * np.exp(17.27 * tc / (tc + 237.3))
            ea = 0.6108 * np.exp(17.27 * dc / (dc + 237.3))
            # Period VI is a representative composite, not a mean of native matches.
            empty = np.full(n, np.nan)
            a = np.column_stack(
                [
                    tc,
                    d["tp"],
                    d["ssrd"] / 10800,
                    np.maximum(es - ea, 0),
                    np.clip(ea / es, 0, 1),
                    ppfd(d["ssrd"]),
                    np.hypot(d["u10"], d["v10"]),
                    empty,
                    empty,
                    rootzone(soil),
                ]
            )
            good = np.isfinite(a)
            sums += np.where(good, a, 0)
            counts += good
        mean = np.divide(sums, counts, out=np.full_like(sums, np.nan), where=counts > 0)
        mean[:, 1] = np.where(counts[:, 1] == len(times), sums[:, 1], np.nan)
        a = pd.DataFrame(mean, columns=DYNAMIC)
        a["index"] = self.points["index"].to_numpy()
        a["period_id"] = int(row.period_id)
        a["period_key"] = row.period_key
        a["expected_steps"] = len(times)
        for j, k in enumerate(DYNAMIC):
            a[k + "_support"] = counts[:, j]
        center = self.vi.at(pd.Timestamp(row.period_key, tz="UTC"))
        for k in ("EVI", "NDVI"):
            a[k] = center[k]
            a[k + "_support"] = np.isfinite(center[k]).astype(int)
            a[k + "_filled"] = center[k + "_filled"]
        a["vi_date"] = center["vi_date"]
        a["vi_sensor"] = center["vi_sensor"]
        year = (
            int(row.hydrological_year)
            if hasattr(row, "hydrological_year") and pd.notna(row.hydrological_year)
            else row.start.year + int(row.start.strftime("%m-%d") >= "07-16")
        )
        path = self.sources.annual(year)
        if year not in self.proxy_static:
            fields = [
                ("LC_proxy", 1),
                ("year_since_disturb", 2),
                ("agb", 3),
                ("PH", 8),
                ("N", 9),
                ("SOC", 10),
                ("Sand", 11),
                ("DEM", 15),
            ]
            values = sample_many(
                path, self.points.lon, self.points.lat, [b for _, b in fields]
            )
            self.proxy_static[year] = {
                name: values[:, j] for j, (name, _) in enumerate(fields)
            }
        validate_lc(self.proxy_static[year]["LC_proxy"], "annual proxy LC")
        for name, value in self.proxy_static[year].items():
            a[name] = value
        a["lat"] = self.points.lat.to_numpy()
        a["lon"] = self.points.lon.to_numpy()
        return a


def add_previous(current, history):
    a = current.copy()
    for k in range(1, 5):
        prev = (
            history[int(a.period_id.iloc[0]) - k].set_index("index").reindex(a["index"])
        )
        for n in DYNAMIC:
            a[f"{n}_L{k}"] = prev[n].to_numpy()
    return a


def validate_lc(values, name):
    values = np.asarray(values)
    unknown = set(values[np.isfinite(values)]) - set(range(12))
    if unknown:
        raise ValueError(f"Unknown {name} encoding: {unknown}")


def native_static(points, year, sources):
    bands = {
        "year_since_disturb": 2,
        "agb": 3,
        "twi": 4,
        "PH": 9,
        "N": 10,
        "SOC": 11,
        "Sand": 12,
    }
    values = sample_many(
        sources.native(year), points.lon, points.lat, list(bands.values())
    )
    out = {n: values[:, j] for j, n in enumerate(bands)}
    out["LC"] = sample_raster(sources.paths["native_lc"], points.lon, points.lat)
    validate_lc(out["LC"], "static native LC")
    codes = sample_raster(sources.paths["kgc"], points.lon, points.lat)
    legend = pd.read_csv(sources.paths["kgc_legend"])
    mapping = dict(zip(legend.code, legend.KGC))
    unknown = set(codes[np.isfinite(codes)]) - set(mapping)
    if unknown:
        raise ValueError(f"Unknown KGC encoding: {unknown}")
    out["KGC"] = [mapping.get(v, None) for v in codes]
    return out

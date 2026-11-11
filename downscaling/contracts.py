"""Feature order, units and missingness contracts shared by all inference paths."""
import numpy as np
import pandas as pd
from rasterio.warp import transform

TARGETS = ["NEE", "LE"]

KGC = [
    "Af",
    "Am",
    "Aw",
    "BSh",
    "BSk",
    "BWh",
    "BWk",
    "Cfa",
    "Cfb",
    "Cfc",
    "Csa",
    "Csb",
    "Cwa",
    "Cwb",
    "Dfa",
    "Dfb",
    "Dfc",
    "Dsb",
    "Dsc",
    "Dwa",
    "Dwb",
    "Dwc",
    "ET",
]

NATIVE = [
    "t2m",
    "sp",
    "d2m",
    "tp",
    "strd",
    "lon",
    "sin_doy",
    "cos_doy",
    "NDVI",
    "EVI",
    "twi",
    "PH",
    "Sand",
    "lat",
    "agb",
    "SOC",
    "N",
    "year_since_disturb",
    "PPFD",
    "WindSpeed",
    "t2m2",
    "PPFD2",
    "EVI2",
    "VPD_t2m",
    "sincos",
    "sm_0_10",
    "sm_10_40",
    "sm_40_100",
    "sm_100_200",
    "VPD",
    "is_day",
]

SOIL = ["sm_0_10", "sm_10_40", "sm_40_100", "sm_100_200"]

DYNAMIC = [
    "temp_c",
    "precipitation_m",
    "shortwave_w_m2",
    "vpd_kpa",
    "rh_fraction",
    "ppfd_umol_m2_s",
    "wind_speed_m_s",
    "EVI",
    "NDVI",
    "gldas_rootzone_vwc",
]

STATIC=['year_since_disturb','agb','PH','N','SOC','Sand','lat','lon','DEM']

TIME=['time_sin_k1','time_cos_k1','time_sin_k4','time_cos_k4','photoperiod_hours','projected_x_m','projected_y_m','hydrological_year_continuous']

NUMERIC=STATIC+DYNAMIC+TIME+[f'{n}_L{k}' for k in range(1,5) for n in DYNAMIC]

CEA='+proj=cea +lat_ts=30 +lon_0=0 +x_0=0 +y_0=0 +datum=WGS84 +units=m +no_defs'

def unique(frame, keys):
    if frame[keys].isna().any().any() or frame.duplicated(keys).any():
        raise ValueError(f"Missing or duplicate keys: {keys}")

def rootzone(layers):
    a = np.asarray(layers, dtype=float)
    if a.shape[-1] != 4:
        raise ValueError("Four GLDAS layers required")
    # Sum, not nansum: an absent constituent does not mean dry soil.
    return a[..., :3].sum(axis=-1) / 1000.0

def ppfd(shortwave_j_m2):
    return np.asarray(shortwave_j_m2) / 10800.0 * 0.45 * 4.57

def short_gaps(values, dates, sources, maximum=2):
    """Fill bounded gaps on a COMPLETE composite calendar, same sensor only."""
    a = np.asarray(values, dtype=float).copy()
    t = pd.DatetimeIndex(dates).asi8
    s = np.asarray(sources)
    flag = np.zeros(a.shape, dtype=np.uint8)
    if len(a) != len(t) or len(s) != len(t) or np.any(np.diff(t) <= 0):
        raise ValueError("Invalid VI calendar")
    i = 0
    while i < len(a):
        if np.isfinite(a[i]):
            i += 1
            continue
        j = i
        while j < len(a) and not np.isfinite(a[j]):
            j += 1
        if (
            i > 0
            and j < len(a)
            and j - i <= maximum
            and np.all(s[i - 1 : j + 1] == s[i - 1])
        ):
            a[i:j] = np.interp(
                t[i:j].astype(float), t[[i - 1, j]].astype(float), a[[i - 1, j]]
            )
            flag[i:j] = 1
        i = j
    return a, flag

def aggregate_period(frame, start, stop):
    f = frame[(frame.utc >= start) & (frame.utc < stop)].copy()
    unique(f, ["index", "utc"])
    if f.empty:
        raise ValueError("No source rows in period")
    expected = int((stop - start).total_seconds() / 10800)
    if f.groupby("index").size().ne(expected).any():
        raise ValueError("Incomplete source time inventory")
    f["gldas_rootzone_vwc"] = rootzone(f[SOIL])
    tc = f.t2m - 273.15
    dc = f.d2m - 273.15
    es = 0.6108 * np.exp(17.27 * tc / (tc + 237.3))
    ea = 0.6108 * np.exp(17.27 * dc / (dc + 237.3))
    f["temp_c"] = tc
    f["precipitation_m"] = f.tp
    f["shortwave_w_m2"] = f.ssrd / 10800
    f["vpd_kpa"] = np.maximum(es - ea, 0)
    f["rh_fraction"] = np.clip(ea / es, 0, 1)
    f["ppfd_umol_m2_s"] = ppfd(f.ssrd)
    f["wind_speed_m_s"] = np.hypot(f.u10, f.v10)
    out = f.groupby("index")[DYNAMIC].mean()
    # Missing precipitation samples invalidate the total; partial sums are not totals.
    out["precipitation_m"] = f.groupby("index").precipitation_m.sum(min_count=expected)
    for n in DYNAMIC:
        out[n + "_support"] = f.groupby("index")[n].count()
    out["expected_steps"] = expected
    return out.reset_index()

def predictors(frame):
    """Same numeric fields for fitting and prediction. No target argument or label reads."""
    f=frame.copy();dates=pd.to_datetime(f.period_key);doy=dates.dt.dayofyear.to_numpy()
    for k in [1,4]:
        f[f'time_sin_k{k}']=np.sin(2*np.pi*k*doy/365.25)
        f[f'time_cos_k{k}']=np.cos(2*np.pi*k*doy/365.25)
    decl=.409*np.sin(2*np.pi*doy/365-1.39)
    f['photoperiod_hours']=24/np.pi*np.arccos(np.clip(-np.tan(np.deg2rad(f.lat))*np.tan(decl),-1,1))
    x,y=transform('EPSG:4326',CEA,f.lon.tolist(),f.lat.tolist())
    f['projected_x_m']=x;f['projected_y_m']=y
    f['hydrological_year_continuous']=dates.dt.year+(dates.dt.strftime('%m-%d')>='07-16').astype(int)
    values=f[NUMERIC].to_numpy(np.float32)
    lc=f.LC_proxy.to_numpy(float)
    valid=np.isfinite(values).all(axis=1)&np.isfinite(lc)&(lc>=0)&(lc<=11)&(lc==np.floor(lc))
    return values,lc,valid

"""Label-free proxy, native correction and train-side spatial lookup."""

import numpy as np
import pandas as pd
import torch
import xgboost as xgb
from sklearn.neighbors import BallTree
from downscaling.contracts import KGC, NATIVE, NUMERIC, TARGETS, predictors
from downscaling.artifacts import read
from downscaling.models.bundle import validate
from downscaling.models.network import ProxyNetwork
from pathlib import Path


def native_matrix(frame):
    """Use the trained float32 feature order; NaN continuous values stay missing."""
    kg = frame.KGC.map({v: i for i, v in enumerate(KGC)}).to_numpy(float)
    lc = frame.LC.to_numpy(float)
    unknown_kg = set(frame.KGC.dropna()) - set(KGC)
    # Known map KGC classes absent from training are excluded before this interface.
    if unknown_kg:
        raise ValueError(f"Unsupported KGC in model evaluation: {unknown_kg}")
    if np.any(np.isfinite(lc) & ~np.isin(lc, np.arange(1, 9))):
        raise ValueError("Unsupported LC in model evaluation")
    if not (np.isfinite(kg).all() and np.isfinite(lc).all()):
        raise ValueError("Missing category in model evaluation")
    a = frame[NATIVE].to_numpy(np.float32)
    return np.concatenate(
        [
            a[:, :5],
            np.eye(23, dtype=np.float32)[kg.astype(int)],
            a[:, 5:],
            np.eye(8, dtype=np.float32)[lc.astype(int) - 1, 1:],
        ],
        axis=1,
    )


def average_finite(a, axis=0):
    ok = np.isfinite(a)
    n = ok.sum(axis=axis)
    s = np.where(ok, a, 0).sum(axis=axis)
    return np.divide(s, n, out=np.full_like(s, np.nan, dtype=float), where=n > 0)


class Spatial:
    def __init__(self, path):
        with np.load(path) as archive:
            self.z = {name: archive[name] for name in archive.files}
        self.train = np.isin(self.z["site_ids"], self.z["train_sites"])
        self.trees = {}
        self.indices = {}
        self.global_mean = average_finite(self.z["cell"][self.train])
        self.fallback = {}
        for lc in range(1, 9):
            ids = np.flatnonzero(self.train & (self.z["lc"] == lc))
            self.indices[lc] = ids
            local = (
                average_finite(self.z["cell"][ids]) if len(ids) else self.global_mean
            )
            self.fallback[lc] = np.where(np.isfinite(local), local, self.global_mean)
            if len(ids):
                self.trees[lc] = BallTree(
                    np.deg2rad(self.z["coords"][ids]), metric="haversine"
                )

    def query(self, points, lc):
        n = len(points)
        out = np.full((n, 96, 2), np.nan)
        distance = np.full(n, np.nan)
        for code in np.unique(lc):
            if code not in range(1, 9):
                raise ValueError("Unknown spatial LC")
            take = np.flatnonzero(lc == code)
            ids = self.indices[code]
            base = self.fallback[code]
            for start in range(0, len(take), 512):
                ix = take[start : start + 512]
                if len(ids):
                    d, j = self.trees[code].query(
                        np.deg2rad(points[["lat", "lon"]].to_numpy()[ix]),
                        k=min(20, len(ids)),
                    )
                    value = average_finite(self.z["cell"][ids[j]], axis=1)
                    distance[ix] = d[:, -1] * 6371.0088
                    out[ix] = np.where(np.isfinite(value), value, base)
                else:
                    out[ix] = base
        return np.clip(out, -self.z["caps"], self.z["caps"]), distance


class Accumulator:
    """Float64 (fold, pixel, target) sums and shared uint32 support counts."""

    def __init__(self, n):
        self.sum = np.zeros((3, n, 2), np.float64)
        self.count = np.zeros(n, np.uint32)

    def add(self, values, ix):
        if values.shape != (3, len(ix), 2) or not np.isfinite(values).all():
            raise ValueError("Invalid aggregate input")
        self.sum[:, ix] += values
        self.count[ix] += 1

    def finish(self):
        means = np.divide(
            self.sum,
            self.count[None, :, None],
            out=np.full_like(self.sum, np.nan),
            where=self.count[None, :, None] > 0,
        )
        return means, means.mean(axis=0), means.std(axis=0, ddof=0)


class Models:
    def __init__(self, root, device="cpu", cpu_threads=1):
        self.root = Path(root)
        self.manifest = validate(root)
        self.device = device
        if device not in ("cpu", "cuda"):
            raise ValueError("Choose cpu or cuda")
        if device == "cuda" and not torch.cuda.is_available():
            raise RuntimeError("CUDA requested but no CUDA device is available")
        if device == "cuda":
            import cupy as cp

            if cp.cuda.runtime.getDeviceCount() != 1:
                raise RuntimeError("Exactly one visible CUDA device required")
        torch.set_num_threads(cpu_threads)
        self.proxies = {}
        self.metadata = {}
        self.means = {}
        self.spatial = {}
        for fold in (1, 2, 3):
            metadata = read(self.root / f"proxy/fold_{fold}.json")
            if metadata["preprocessing"]["numeric_names"] != NUMERIC or metadata[
                "preprocessing"
            ]["lc_levels"] != list(range(12)):
                raise ValueError("Proxy feature/category order mismatch")
            candidate = metadata["candidate"]
            model = ProxyNetwork(
                len(NUMERIC),
                int(candidate["width"]),
                int(candidate["blocks"]),
                candidate["sharing"],
                "student_t",
                metadata["settings"]["quantiles"],
                use_year_embedding=bool(candidate["use_year_embedding"]),
            )
            state = torch.load(
                self.root / f"proxy/fold_{fold}.pt",
                map_location="cpu",
                weights_only=True,
            )
            model.load_state_dict(state, strict=True)
            model.to(device)
            model.eval()
            self.proxies[fold] = model
            self.metadata[fold] = metadata
            self.spatial[fold] = Spatial(self.root / f"spatial/fold_{fold}.npz")
            for target in TARGETS:
                mean = xgb.Booster()
                mean.load_model(self.root / f"mean/fold_{fold}_{target}.json")
                mean.set_param(dict(device=device, nthread=cpu_threads))
                self.means[fold, target] = mean

    def proxy(self, frame):
        raw, lc, valid = predictors(frame)
        output = np.full((3, len(frame), 2), np.nan, np.float32)
        if not valid.any():
            return output, valid
        years = pd.to_datetime(frame.period_key).dt.year.to_numpy()
        for fold in (1, 2, 3):
            metadata = self.metadata[fold]
            pre = metadata["preprocessing"]
            numeric = (
                (raw[valid] - np.asarray(pre["numeric_mean"]))
                / np.asarray(pre["numeric_std"])
            ).astype(np.float32)
            categories = lc[valid].astype(np.int64)
            year_ids = np.array(
                [
                    int(y) - 2012 if y in pre["fit_years"] and 2013 <= y <= 2024 else 0
                    for y in years[valid]
                ],
                np.int64,
            )
            pieces = []
            with torch.inference_mode():
                for begin in range(0, len(numeric), 8192):
                    end = begin + 8192
                    prediction = self.proxies[fold](
                        torch.as_tensor(numeric[begin:end], device=self.device),
                        torch.as_tensor(categories[begin:end], device=self.device),
                        torch.as_tensor(year_ids[begin:end], device=self.device),
                    )
                    pieces.append(
                        prediction[:, :, 0].cpu().numpy()
                        * np.asarray(metadata["target_scale"])
                        + np.asarray(metadata["target_center"])
                    )
            output[fold - 1, valid] = np.concatenate(pieces)
        if not np.isfinite(output[:, valid]).all():
            raise ValueError("Nonfinite eligible proxy prediction")
        return output, valid

    def native(self, frame, proxy_mu, bias):
        matrix = native_matrix(frame)
        if self.device == "cuda":
            import cupy as cp

            matrix = cp.asarray(matrix)
        output = np.empty((3, len(frame), 2), np.float32)
        time = pd.DatetimeIndex(frame.utc)
        calendar = (time.month - 1) * 8 + time.hour // 3
        for fold in (1, 2, 3):
            for target_index, target in enumerate(TARGETS):
                residual = self.means[fold, target].inplace_predict(matrix)
                if self.device == "cuda":
                    residual = residual.get()
                output[fold - 1, :, target_index] = (
                    proxy_mu[fold - 1, :, target_index]
                    - residual
                    - bias[fold - 1, np.arange(len(frame)), calendar, target_index]
                )
        if not np.isfinite(output).all():
            raise ValueError("Nonfinite eligible native prediction")
        return output

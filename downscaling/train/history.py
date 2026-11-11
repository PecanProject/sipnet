"""Optional historical scoring with explicit site, fold and UTC correspondence."""

from pathlib import Path
import numpy as np
import pandas as pd
from downscaling.artifacts import digest, read, sha, write
from downscaling.contracts import TARGETS
from downscaling.train.splits import align_ids
from downscaling.train.evaluate import contributions, summary

ARRAYS = ("truth", "error", "calibrated_scale", "nearby_calendar_LC", "eligible")


class Historical:
    def __init__(self, root):
        self.root = Path(root).resolve()
        self.manifest = read(self.root / "manifest.json")
        expected = {
            "sites.csv",
            "steps.csv",
            "thresholds.csv",
            *[f"{key}.npy" for key in ARRAYS],
        }
        payload = {key: value for key, value in self.manifest.items() if key != "id"}
        if (
            self.manifest.get("schema") != "sipnet-downscaling-history-1"
            or digest(payload) != self.manifest.get("id")
            or set(self.manifest["files"]) != expected
        ):
            raise ValueError("Invalid historical reference manifest")
        for name, checksum in self.manifest["files"].items():
            path = (self.root / name).resolve()
            if (
                not path.is_relative_to(self.root)
                or not path.is_file()
                or sha(path) != checksum
            ):
                raise ValueError(f"Missing or changed historical artifact: {name}")
        sites = pd.read_csv(self.root / "sites.csv")
        self.ids = pd.Index(sites["index"])
        align_ids(self.ids, self.ids, "historical sites")
        self.folds = sites.fold.to_numpy()
        if not np.isin(self.folds, (1, 2, 3)).all():
            raise ValueError("Unknown historical fold")
        self.folds = self.folds.astype(np.int64)
        self.times = pd.DatetimeIndex(
            pd.to_datetime(pd.read_csv(self.root / "steps.csv").utc, utc=True)
        )
        align_ids(self.times, self.times, "historical UTC steps")
        if (
            not self.times.is_monotonic_increasing
            or (self.times.minute != 0).any()
            or (self.times.second != 0).any()
            or (self.times.hour % 3 != 0).any()
        ):
            raise ValueError("Invalid historical UTC axis")
        self.arrays = {
            key: np.load(self.root / f"{key}.npy", mmap_mode="r", allow_pickle=False)
            for key in ARRAYS
        }
        for key, value in self.arrays.items():
            shape = (
                (len(self.ids), 96, 2)
                if key == "nearby_calendar_LC"
                else (len(self.ids), len(self.times))
                + (() if key == "eligible" else (2,))
            )
            if value.shape != shape or (key == "eligible" and value.dtype != bool):
                raise ValueError(f"Historical array/ID mismatch: {key}")
        thresholds = pd.read_csv(self.root / "thresholds.csv")
        if (
            thresholds[["fold", "target"]].isna().any().any()
            or thresholds.duplicated(["fold", "target"]).any()
        ):
            raise ValueError("Duplicate or missing historical threshold ID")
        thresholds = thresholds.set_index(["fold", "target"])
        keys = pd.MultiIndex.from_product(
            [[1, 2, 3], TARGETS], names=["fold", "target"]
        )
        if set(thresholds.index) != set(keys):
            raise ValueError("Missing historical fold/target threshold")
        self.caps = thresholds.reindex(keys).cap.to_numpy().reshape(3, 2)
        if not np.isfinite(self.caps).all() or np.any(self.caps < 0):
            raise ValueError("Invalid historical thresholds")

    def values(self, site_ids, utc):
        sites = self.ids.get_indexer(site_ids)
        if np.any(sites < 0):
            raise ValueError("Missing historical site IDs")
        utc = pd.DatetimeIndex(utc)
        steps = self.times.get_indexer(utc)
        supported = steps >= 0
        truth = np.full((len(sites), 2), np.nan)
        location, scale = np.full_like(truth, np.nan), np.full_like(truth, np.nan)
        take = np.flatnonzero(supported)
        s, t = sites[take], steps[take]
        bins = (utc.month.to_numpy()[take] - 1) * 8 + utc.hour.to_numpy()[take] // 3
        truth[take] = self.arrays["truth"][s, t]
        bias = self.arrays["nearby_calendar_LC"][s, bins]
        caps = self.caps[self.folds[s] - 1]
        location[take] = (
            truth[take] + self.arrays["error"][s, t] - np.clip(bias, -caps, caps)
        )
        scale[take] = self.arrays["calibrated_scale"][s, t]
        supported[take] &= self.arrays["eligible"][s, t]
        supported &= (
            np.isfinite(truth).all(axis=1)
            & np.isfinite(location).all(axis=1)
            & np.isfinite(scale).all(axis=1)
            & (scale > 0).all(axis=1)
        )
        return truth, location, scale, supported


def compare(run, reference, output):
    reference = Historical(reference)
    output = Path(output).resolve()
    for source in (
        run.root,
        run.data.root,
        reference.root,
        Path(__file__).resolve().parents[2],
    ):
        if output.is_relative_to(source) or source.is_relative_to(output):
            raise ValueError(
                "Historical comparison output must be separate from inputs and code"
            )
    if output.exists() and any(output.iterdir()):
        raise ValueError("Choose an empty historical comparison output directory")
    # Resolve all sites before calculating anything; never permit a -1 index.
    align_ids(reference.ids, run.data.arrays["site_ids"], "historical requested sites")
    stats = {
        (model, target): np.zeros(11)
        for model in ("current", "reference")
        for target in TARGETS
    }
    for shard in run.data.shards:
        current = run.load(f'predictions/final/{shard["name"]}.npz')
        sites = run.data.arrays["site_ids"][current["period_row"] % run.data.n]
        truth, location, scale, common = reference.values(
            sites, run.data.times(current["step"])
        )
        current_truth = run.data.native(shard, "y")
        for target, label in enumerate(TARGETS):
            for model, y, mu, sigma in [
                ("current", current_truth, current["location"], current["scale"]),
                ("reference", truth, location, scale),
            ]:
                stats[model, label] += contributions(
                    y[common, target], mu[common, target], sigma[common, target]
                ).sum(axis=0)
    if not all(value[0] > 0 for value in stats.values()):
        raise ValueError("No common supported historical samples")
    table = pd.DataFrame(
        [
            dict(model=model, target=target, **summary(value))
            for (model, target), value in stats.items()
        ]
    )
    output.mkdir(parents=True, exist_ok=True)
    table.to_csv(output / "metrics.csv", index=False)
    write(
        output / "comparison.json",
        dict(
            status="complete",
            training_token=run.token,
            reference_id=reference.manifest["id"],
            metrics_sha256=sha(output / "metrics.csv"),
            interpretation="Identical native keys; each product uses its own truth. Differences are not independent-observation skill gains.",
        ),
    )
    return dict(status="complete", output=str(output))

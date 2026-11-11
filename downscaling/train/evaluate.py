"""Train-only spatial calibration and unchanged Student-t scoring formulas."""

import numpy as np
import pandas as pd
from scipy.stats import t
from scipy.special import beta
from sklearn.neighbors import BallTree
from downscaling.artifacts import receipt
from downscaling.contracts import TARGETS
from downscaling.models.inference import average_finite
from downscaling.train.recipe import Q90


def weighted_quantile(a, w, q=0.999):
    ok = np.isfinite(a) & (w > 0)
    a = np.asarray(a)[ok]
    w = np.asarray(w)[ok]
    if not len(a):
        raise ValueError("No supported clipping values")
    order = np.argsort(a)
    a = a[order]
    cw = np.cumsum(w[order], dtype=float)
    return a[min(np.searchsorted(cw, q * cw[-1], side="left"), len(a) - 1)]


def smooth(cell, train, query, coords, lc, exclude_self=False):
    pred = np.full((len(cell), 96, 2), np.nan)
    distance = np.full(len(cell), np.nan)
    global_sum = np.nansum(cell[train], axis=0)
    global_n = np.isfinite(cell[train]).sum(axis=0)
    for c in np.unique(lc[query]):
        tr = np.flatnonzero(train & (lc == c))
        te = np.flatnonzero(query & (lc == c))
        local_sum = np.nansum(cell[tr], axis=0)
        local_n = np.isfinite(cell[tr]).sum(axis=0)
        if len(tr):
            dist, near = BallTree(np.deg2rad(coords[tr]), metric="haversine").query(
                np.deg2rad(coords[te]), k=min(21 if exclude_self else 20, len(tr))
            )
        for j, site in enumerate(te):
            own = (
                cell[site] if exclude_self and train[site] else np.full((96, 2), np.nan)
            )

            def base(s, n):
                nn = n - np.isfinite(own)
                ss = s - np.nan_to_num(own)
                return np.divide(ss, nn, out=np.full_like(ss, np.nan), where=nn > 0)

            global_mean = base(global_sum, global_n)
            local = base(local_sum, local_n)
            fallback = np.where(np.isfinite(local), local, global_mean)
            if len(tr):
                indices = tr[near[j]]
                dd = dist[j]
                if exclude_self:
                    keep = indices != site
                    indices = indices[keep]
                    dd = dd[keep]
                indices = indices[:20]
                dd = dd[:20]
                value = average_finite(cell[indices]) if len(indices) else fallback
                if len(dd):
                    distance[site] = dd[-1] * 6371.0088
                pred[site] = np.where(np.isfinite(value), value, fallback)
            else:
                pred[site] = fallback
    return pred, distance


def contributions(y, mu, scale):
    y = np.asarray(y, float)
    mu = np.asarray(mu, float)
    s = np.asarray(scale, float)
    e = mu - y
    z = (y - mu) / s
    const = 2 * np.sqrt(4) / 3 * beta(0.5, 3.5) / beta(0.5, 2) ** 2
    crps = s * (z * (2 * t.cdf(z, 4) - 1) + 2 * t.pdf(z, 4) * (4 + z * z) / 3 - const)
    return np.column_stack(
        [
            np.ones(len(y)),
            y,
            y * y,
            e * e,
            np.abs(e),
            e,
            crps,
            np.abs(e) <= t.ppf(0.75, 4) * s,
            np.abs(e) <= Q90 * s,
            2 * t.ppf(0.75, 4) * s,
            2 * Q90 * s,
        ]
    )


def summary(a):
    n = a[0]
    sst = a[2] - a[1] ** 2 / n
    return dict(
        rows=int(n),
        rmse=np.sqrt(a[3] / n),
        r2=1 - a[3] / sst if sst > 0 else None,
        mae=a[4] / n,
        bias=a[5] / n,
        crps=a[6] / n,
        coverage_50=a[7] / n,
        coverage_90=a[8] / n,
        width_50=a[9] / n,
        width_90=a[10] / n,
    )


def add(stats, key, values, mask):
    if np.any(mask):
        stats.setdefault(key, np.zeros(11))[:] += values[mask].sum(axis=0)


def spatial(run, outer):
    """Float64 residual cells (site, month * eight UTC steps, target)."""
    data = run.data
    folds = data.folds
    outer_folds = folds.fold.to_numpy()
    sums = np.zeros((data.n, 96, 2), np.float64)
    counts = np.zeros((data.n, 96), np.int64)
    for shard in data.shards:
        nested = run.load(f'cache/nested_o{outer}/{shard["name"]}.npz')
        offsets = nested["offset"]
        rows = data.native(shard, "row")[offsets]
        steps = data.native(shard, "step")[offsets]
        if (outer_folds[rows % data.n] == outer).any():
            raise ValueError("Outer holdout in spatial calibration")
        residual = nested["mu"].astype(float) - data.native(shard, "y")[offsets]
        groups = (rows % data.n) * 96 + data.calendar_bins(steps)
        counts.ravel()[:] += np.bincount(groups, minlength=data.n * 96)
        for target in range(2):
            sums[:, :, target] += np.bincount(
                groups, weights=residual[:, target], minlength=data.n * 96
            ).reshape(data.n, 96)
    cells = np.divide(
        sums,
        counts[:, :, None],
        out=np.full_like(sums, np.nan),
        where=counts[:, :, None] > 0,
    )
    coordinates = folds[["lat", "lon"]].to_numpy()
    lc = folds.LC.to_numpy()
    train = (
        (outer_folds != outer)
        & (counts.sum(axis=1) > 0)
        & np.isfinite(coordinates).all(axis=1)
    )
    test = (outer_folds == outer) & np.isfinite(coordinates).all(axis=1)
    prediction, distance = smooth(cells, train, test, coordinates, lc)
    leave_one_out, _ = smooth(cells, train, train, coordinates, lc, True)
    caps = np.array(
        [
            weighted_quantile(
                np.abs(leave_one_out[train, :, target]).ravel(), counts[train].ravel()
            )
            for target in range(2)
        ]
    )
    run.save(
        f"models/spatial_o{outer}.npz",
        cell=cells,
        counts=counts,
        bias=prediction,
        caps=caps,
        distance_km=distance,
        train_sites=folds.loc[train, "index"].to_numpy(),
        site_ids=folds["index"].to_numpy(),
        lc=lc,
        coords=coordinates,
    )
    run.json(
        f"validation/spatial_o{outer}.json",
        dict(
            status="passed",
            fold=outer,
            caps=caps,
            train_rows=int(counts.sum()),
            leave_self_out=True,
            training_only=True,
            neighbors=20,
            radius_limit=None,
        ),
    )


def report_outputs(data):
    return [
        "REPORT.md",
        "tables/metrics.csv",
        "predictions/period_oof.h5",
        "predictions/month_oof.h5",
        *[f'predictions/final/{shard["name"]}.npz' for shard in data.shards],
    ]


def report(run):
    """Score held-out native predictions and support-weighted aggregate means.

    Interval multipliers stay fixed when shifting the mean. Aggregated scales
    retain the shared-quantile diagnostic interpretation, not a calibrated CI.
    """
    data = run.data
    folds = data.folds.fold.to_numpy()
    spatial_models = {
        outer: run.load(f"models/spatial_o{outer}.npz") for outer in (1, 2, 3)
    }
    stop = pd.Timestamp(data.periods.end_date.iloc[-1], tz="UTC") + pd.Timedelta(days=1)
    times = pd.date_range(data.start, stop, freq="3h", inclusive="left")
    month_codes, months = pd.factorize(times.strftime("%Y-%m"), sort=True)
    counts = np.zeros(data.rows, np.int32)
    sums = np.zeros((data.rows, 2, 3), np.float64)
    monthly_counts = np.zeros(len(months) * data.n, np.int32)
    monthly_sums = np.zeros((len(monthly_counts), 2, 3), np.float64)
    stats = {}
    total = 0
    for shard in data.shards:
        rows = data.native(shard, "row")
        steps = data.native(shard, "step")
        sites = rows % data.n
        calendar = data.calendar_bins(steps)
        truth = np.asarray(data.native(shard, "y"))
        routes = data.native(shard, "route")
        location, scale = np.full_like(truth, np.nan), np.full_like(truth, np.nan)
        distance = np.full(len(rows), np.nan)
        for outer in (1, 2, 3):
            offsets = np.flatnonzero(folds[sites] == outer)
            if not len(offsets):
                continue
            model = spatial_models[outer]
            distance[offsets] = model["distance_km"][sites[offsets]]
            for target, label in enumerate(TARGETS):
                mean = run.load(
                    f'predictions/mean_o{outer}_j0_{label}/{shard["name"]}.npz'
                )
                quantile = run.load(
                    f'predictions/quantile_o{outer}_j0_{label}/{shard["name"]}.npz'
                )
                np.testing.assert_array_equal(offsets, mean["offset"])
                np.testing.assert_array_equal(offsets, quantile["offset"])
                bias = np.clip(
                    model["bias"][sites[offsets], calendar[offsets], target],
                    -model["caps"][target],
                    model["caps"][target],
                )
                location[offsets, target] = mean["mu"] - bias
                scale[offsets, target] = mean["scale"] * quantile["k"]
        if (
            not np.isfinite(location).all()
            or not (np.isfinite(scale) & (scale > 0)).all()
        ):
            raise ValueError(f'Invalid final out-of-fold prediction: {shard["name"]}')
        run.save(
            f'predictions/final/{shard["name"]}.npz',
            period_row=rows,
            step=steps,
            location=location,
            scale=scale,
            distance_km=distance,
        )
        restored = run.load(f'predictions/final/{shard["name"]}.npz')
        np.testing.assert_array_equal(location, restored["location"])
        for target, label in enumerate(TARGETS):
            values = contributions(
                truth[:, target], location[:, target], scale[:, target]
            )
            for outer in (0, 1, 2, 3):
                mask = np.ones(len(rows), bool) if outer == 0 else folds[sites] == outer
                add(stats, ("3h", label, f"fold{outer}"), values, mask)
            dominant = np.argmax(routes[:, target], axis=1) + 1
            dominant[np.max(routes[:, target], axis=1) < 1] = 4
            for route in (1, 2, 3, 4):
                add(stats, ("3h", label, f"route{route}"), values, dominant == route)
            for lc in range(1, 9):
                add(
                    stats,
                    ("3h", label, f"LC{lc}"),
                    values,
                    data.folds.LC.to_numpy()[sites] == lc,
                )
            add(stats, ("3h", label, "neighbor_gt1000km"), values, distance > 1000)
            add(stats, ("3h", label, "neighbor_le1000km"), values, distance <= 1000)
        period_rows, inverse = np.unique(rows, return_inverse=True)
        counts[period_rows] += np.bincount(inverse).astype(np.int32)
        monthly_group = month_codes[steps] * data.n + sites
        month_rows, month_inverse = np.unique(monthly_group, return_inverse=True)
        monthly_counts[month_rows] += np.bincount(month_inverse).astype(np.int32)
        for target in range(2):
            for field, values in enumerate((truth, location, scale)):
                sums[period_rows, target, field] += np.bincount(
                    inverse, weights=values[:, target]
                )
                monthly_sums[month_rows, target, field] += np.bincount(
                    month_inverse, weights=values[:, target]
                )
        total += len(rows)
        print(f'SCORED {shard["name"]} {total}', flush=True)
    import h5py

    for label, count, summed, keys in [
        ("period", counts, sums, data.periods.period_key.tolist()),
        ("month", monthly_counts, monthly_sums, list(months)),
    ]:
        means = np.divide(
            summed,
            count[:, None, None],
            out=np.full_like(summed, np.nan),
            where=count[:, None, None] > 0,
        )
        path = run.path(f"predictions/{label}_oof.h5")
        partial = path.with_suffix(".partial")
        with h5py.File(partial, "w") as handle:
            handle.attrs["token"] = run.token
            handle.attrs["probability"] = (
                "shared-quantile diagnostic; not calibrated aggregate probabilities"
            )
            handle.attrs["value_fields"] = "Y_mean,location,mean_scale"
            handle.attrs["targets"] = "NEE,LE"
            handle.create_dataset(
                "values",
                data=means.reshape(len(keys), data.n, 2, 3),
                compression="gzip",
            )
            handle.create_dataset("support", data=count.reshape(len(keys), data.n))
            handle.create_dataset("site_ids", data=data.arrays["site_ids"])
            handle.create_dataset("keys", data=np.array(keys, dtype="S"))
            if label == "period":
                handle.create_dataset(
                    "expected_support", data=data.periods.expected_3hour_steps
                )
                handle.create_dataset("period_ids", data=data.periods.period_id)
        partial.replace(path)
        receipt(path, run.token)
        with h5py.File(path) as handle:
            np.testing.assert_array_equal(handle["support"][:].ravel(), count)
            np.testing.assert_array_equal(
                handle["values"][:], means.reshape(len(keys), data.n, 2, 3)
            )
        valid = count > 0
        for target, target_label in enumerate(TARGETS):
            values = contributions(
                means[valid, target, 0],
                means[valid, target, 1],
                means[valid, target, 2],
            )
            for outer in (0, 1, 2, 3):
                mask = (
                    np.ones(valid.sum(), bool)
                    if outer == 0
                    else np.tile(folds, len(keys))[valid] == outer
                )
                add(stats, (label, target_label, f"fold{outer}"), values, mask)
    if not total:
        raise ValueError("No supported predictions to score")
    table = pd.DataFrame(
        [
            dict(
                model="SIPNET Downscaling",
                scale=key[0],
                target=key[1],
                sample_set="all",
                stratum=key[2],
                **summary(value),
            )
            for key, value in stats.items()
        ]
    )
    table.to_csv(run.path("tables/metrics.csv"), index=False)
    receipt(run.root / "tables/metrics.csv", run.token)
    text = (
        "SIPNET Downscaling: site-held-out training evaluation\n\n"
        + table[table.stratum == "fold0"].to_csv(index=False)
        + "\nScaling, interval and spatial calibration use fitting sites only. All finite routes are retained. "
        "Aggregate scales use the shared-quantile assumption and are diagnostic, not calibrated probabilities.\n"
    )
    run.path("REPORT.md").write_text(text)
    receipt(run.root / "REPORT.md", run.token)

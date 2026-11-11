"""Bounded CPU preparation feeds one model consumer on the selected device."""

import os
import time
from concurrent.futures import ProcessPoolExecutor
from multiprocessing import get_context
import numpy as np
import pandas as pd
from downscaling.artifacts import read, write, receipt, valid, require, save_npz
from downscaling.contracts import predictors, NUMERIC, KGC
from downscaling.models.inference import Models, Accumulator
from downscaling.prepare.sources import Sources
from downscaling.prepare.domain import points_for_tile, timezones
from downscaling.prepare.vegetation import Vegetation
from downscaling.prepare.features import FeatureStream, add_previous
from downscaling.prepare import soil
from downscaling.predict.state import plan, budget, locked, verify_ledger


def block_base(config, tile):
    return config.destination(f"blocks/r{tile['row']}_c{tile['col']}")


def frame_save(path, frame, token):
    path.parent.mkdir(parents=True, exist_ok=True)
    tmp = path.with_name(path.name + ".partial")
    frame.to_parquet(tmp, index=False)
    tmp.replace(path)
    receipt(path, token)


def period_block(models, points, row, panel, stream, base, token):
    path = base / f"period_{int(row.period_id):03d}.npz"
    if valid(path, token):
        if not valid(base / f"period_{int(row.period_id):03d}_qa.json", token):
            raise ValueError("Missing block QA receipt")
        return
    proxy_mu, ok = models.proxy(panel)

    numeric, proxy_lc, _ = predictors(panel)
    missing_proxy = {
        name: int((~np.isfinite(numeric[:, j])).sum()) for j, name in enumerate(NUMERIC)
    }
    raw_names = [
        "t2m",
        "sp",
        "d2m",
        "tp",
        "strd",
        "PPFD",
        "NDVI",
        "EVI",
        "sm_0_10",
        "sm_10_40",
        "sm_40_100",
        "sm_100_200",
    ]
    native_missing = {n: 0 for n in raw_names}
    # Values outside the native trained vocabulary are explicitly masked.
    static = stream.static.get(row.start.year)
    if static is None:
        from downscaling.prepare.features import native_static as static_native

        static = static_native(points, row.start.year, stream.sources)
    lc = np.asarray(static["LC"])
    kg = np.asarray(static["KGC"])
    category = np.isin(lc, np.arange(1, 9)) & np.isin(kg, KGC)
    ix = np.flatnonzero(ok & category)
    bias = np.zeros((3, len(ix), 96, 2))
    distance = np.full((3, len(points)), np.nan)
    for o in (1, 2, 3):
        if len(ix):
            bias[o - 1], distance[o - 1, ix] = models.spatial[o].query(
                points.iloc[ix], lc[ix]
            )
    acc = Accumulator(len(points))
    months = {}
    monthdiag = {}
    filled = np.zeros(len(points), np.uint16)
    run = np.zeros(len(points), np.uint16)
    longest = run.copy()
    missing_count = np.zeros(len(points), np.uint16)
    times = pd.date_range(row.start, row.stop, freq="3h", inclusive="left")
    for t in times:
        d = stream.at(t)
        for name in raw_names:
            native_missing[name] += int(d[name].isna().sum())
        # LC is static but KGC eligibility is rechecked rather than inferred from labels.
        eligible = ok & np.isin(d.LC, np.arange(1, 9)) & d.KGC.isin(KGC).to_numpy()
        if not np.array_equal(eligible, ok & category):
            raise ValueError("Static native categories changed within period")
        good = np.flatnonzero(eligible)
        run = np.where(eligible, 0, run + 1)
        longest = np.maximum(longest, run)
        missing_count += ~eligible
        filled += np.maximum(d.EVI_filled.to_numpy(), d.NDVI_filled.to_numpy()).astype(
            np.uint16
        )
        month = t.strftime("%Y-%m")
        months.setdefault(month, Accumulator(len(points)))
        if month not in monthdiag:
            monthdiag[month] = dict(
                expected=0,
                filled=np.zeros(len(points), np.uint16),
                longest=np.zeros(len(points), np.uint16),
                prefix=np.zeros(len(points), np.uint16),
                suffix=np.zeros(len(points), np.uint16),
            )
        md = monthdiag[month]
        md["prefix"] += (md["prefix"] == md["expected"]) & ~eligible
        md["expected"] += 1
        md["suffix"] = np.where(eligible, 0, md["suffix"] + 1)
        md["longest"] = np.maximum(md["longest"], md["suffix"])
        md["filled"] += np.maximum(
            d.EVI_filled.to_numpy(), d.NDVI_filled.to_numpy()
        ).astype(np.uint16)
        if len(good):
            val = models.native(
                d.iloc[good].reset_index(drop=True), proxy_mu[:, good], bias
            )
            acc.add(val, good)
            months[month].add(val, good)
    fold, mean, std = acc.finish()
    reason = np.zeros(len(points), np.uint8)
    reason[~ok] |= 1
    reason[~category] |= 2
    arrays = dict(
        pixel=points["index"].to_numpy(),
        fold_mean=fold.astype(np.float32),
        count=acc.count,
        expected=np.int32(len(times)),
        distance_km=distance.astype(np.float32),
        reason=reason,
        vi_filled_steps=filled,
        longest_missing_steps=longest,
        missing_steps=missing_count,
    )
    for month, part in months.items():
        arrays["sum_" + month] = part.sum
        arrays["count_" + month] = part.count
        for name, value in monthdiag[month].items():
            arrays[name + "_" + month] = value
    qa = base / f"period_{int(row.period_id):03d}_qa.json"
    write(
        qa,
        dict(
            period_id=int(row.period_id),
            points=len(points),
            eligible_points=int((acc.count > 0).sum()),
            expected_steps=len(times),
            proxy_missing_by_feature=missing_proxy,
            native_missing_by_feature=native_missing,
            vi_source="Aqua MYD13Q1.061",
            filled_native_pixel_steps=int(filled.sum()),
            far_neighbor_points_by_fold=[int((d > 1000).sum()) for d in distance],
            native_lc_counts=pd.Series(lc)
            .value_counts(dropna=False)
            .rename(index=str)
            .to_dict(),
            proxy_lc_counts=pd.Series(proxy_lc)
            .value_counts(dropna=False)
            .rename(index=str)
            .to_dict(),
        ),
    )
    receipt(qa, token)
    save_npz(path, token, **arrays)


def prepare_tile(config, tile, needed, token):
    import pyarrow as pa

    pa.set_cpu_count(1)
    pa.set_io_thread_count(1)
    started = time.perf_counter()
    sources = Sources(config.sources)
    base = block_base(config, tile)
    points_path = base / "points.parquet"
    excluded_path = base / "excluded.parquet"
    if valid(points_path, token) and valid(excluded_path, token):
        points = pd.read_parquet(points_path)
    else:
        points, _ = points_for_tile(config, sources, tile)
        points["tz"] = timezones(config, points)
        frame_save(excluded_path, points[points.tz.isna()], token)
        points = points[points.tz.notna()].reset_index(drop=True)
        frame_save(points_path, points, token)
    missing = [
        r
        for r in needed.itertuples()
        if not valid(base / f"input_{r.period_id:03d}.parquet", token)
    ]
    if len(points) and missing:
        budget(config, len(points) * len(missing) * 1024)
        vegetation = Vegetation(points, needed.start.min(), needed.stop.max(), sources)
        stream = FeatureStream(
            points, vegetation, soil.Reader(config, points, needed, token), sources
        )
        try:
            for row in missing:
                frame_save(
                    base / f"input_{row.period_id:03d}.parquet",
                    stream.period(row),
                    token,
                )
        finally:
            stream.close()
    write(
        base / "input_timing.json",
        dict(seconds=time.perf_counter() - started, points=len(points)),
    )
    return tile


def block_inventory(config, contract, wanted):
    token = contract["token"]
    sources = Sources(config.sources)
    outputs = []
    by_period = {int(i): [] for i in wanted.period_id}
    for tile in contract["tiles"]:
        base = block_base(config, tile)
        land, _ = points_for_tile(config, sources, tile)
        for name in ("points.parquet", "excluded.parquet"):
            require(base / name, token)
            outputs.append(str((base / name).relative_to(config.output)))
        points = pd.read_parquet(base / "points.parquet")
        excluded = pd.read_parquet(base / "excluded.parquet")
        ids = pd.concat([points["index"], excluded["index"]], ignore_index=True)
        if ids.isna().any() or ids.duplicated().any() or set(ids) != set(land["index"]):
            raise ValueError("Incomplete or overlapping land partition")
        if points.tz.isna().any() or excluded.tz.notna().any():
            raise ValueError("Invalid explicit timezone exclusions")
        if not len(points):
            continue
        for period_id in by_period:
            path = base / f"period_{period_id:03d}.npz"
            qa = base / f"period_{period_id:03d}_qa.json"
            for artifact in (path, qa):
                require(artifact, token)
                outputs.append(str(artifact.relative_to(config.output)))
            with np.load(path) as values:
                np.testing.assert_array_equal(values["pixel"], points["index"])
            if read(qa)["period_id"] != period_id or read(qa)["points"] != len(points):
                raise ValueError("Invalid block QA inventory")
            by_period[period_id].append(path)
    return dict(token=token, periods=list(by_period), outputs=outputs), by_period


def verify_blocks(config, contract, wanted):
    verify_ledger(contract["sources"])
    require(config.destination("blocks_complete.json"), contract["token"])
    expected, blocks = block_inventory(config, contract, wanted)
    if read(config.destination("blocks_complete.json")) != expected:
        raise ValueError("Stale block completion inventory")
    return blocks


def run_blocks(config, contract, needed, wanted):
    token = contract["token"]
    marker = config.destination("blocks_complete.json")
    if marker.exists():
        verify_blocks(config, contract, wanted)
        return
    allocated = int(os.environ.get("NSLOTS", len(os.sched_getaffinity(0))))
    if (config.workers + 1) * config.cpu_threads > allocated:
        raise ValueError("Configured concurrency exceeds current CPU allocation")
    sources = Sources(config.sources)
    for row in needed.itertuples():
        soil.prepare(config, sources, row, token)
    models = Models(config.model_bundle, config.device, config.cpu_threads)
    # The bounded producer queue prevents prepared tiles from accumulating in memory.
    with ProcessPoolExecutor(
        max_workers=config.workers, mp_context=get_context("spawn")
    ) as pool:
        iterator = iter(contract["tiles"])
        pending = []

        def submit():
            tile = next(iterator, None)
            if tile is not None:
                pending.append(pool.submit(prepare_tile, config, tile, needed, token))

        for _ in range(config.workers):
            submit()
        while pending:
            tile = pending.pop(0).result()
            submit()
            base = block_base(config, tile)
            points = pd.read_parquet(base / "points.parquet")
            if not len(points):
                continue
            if all(
                valid(base / f"period_{int(i):03d}.npz", token)
                for i in wanted.period_id
            ):
                for i in wanted.period_id:
                    require(base / f"period_{int(i):03d}_qa.json", token)
                continue
            started = time.perf_counter()
            budget(config, len(points) * len(wanted) * 1024)
            vegetation = Vegetation(
                points, needed.start.min(), needed.stop.max(), sources
            )
            stream = FeatureStream(
                points, vegetation, soil.Reader(config, points, needed, token), sources
            )
            history = {}
            try:
                for row in needed.itertuples():
                    path = base / f"input_{row.period_id:03d}.parquet"
                    require(path, token)
                    frame = pd.read_parquet(path)
                    np.testing.assert_array_equal(frame["index"], points["index"])
                    history[int(row.period_id)] = frame
                    if row.period_id in set(wanted.period_id):
                        period_block(
                            models,
                            points,
                            row,
                            add_previous(frame, history),
                            stream,
                            base,
                            token,
                        )
                        write(
                            config.destination("status.json"),
                            dict(
                                status="running",
                                block=base.name,
                                period_id=int(row.period_id),
                                token=token,
                            ),
                        )
                    for old in list(history):
                        if old < int(row.period_id) - 4:
                            history.pop(old)
            finally:
                stream.close()
            write(
                base / "prediction_timing.json",
                dict(seconds=time.perf_counter() - started, points=len(points)),
            )
    verify_ledger(contract["sources"])
    inventory, _ = block_inventory(config, contract, wanted)
    write(marker, inventory)
    receipt(marker, token)


def execute(command, config, resume=False):
    contract, needed, wanted = plan(config)
    if command == "preflight":
        return dict(
            status="ready", token=contract["token"], tiles=len(contract["tiles"])
        )
    from downscaling.predict.products import export, report

    if command == "report":
        return report(config, contract, wanted)
    with locked(config):
        manifest = config.destination("manifest.json")
        if manifest.exists() and not resume:
            raise ValueError("Existing run requires --resume")
        if not manifest.exists():
            if any(p.name != ".run.lock" for p in config.output.iterdir()):
                raise ValueError("Output directory must be empty for a new run")
            write(manifest, contract)
        started = time.perf_counter()
        run_blocks(config, contract, needed, wanted)
        if config.destination("COMPLETE.json").exists():
            return report(config, contract, wanted)
        export(config, contract, wanted)
        result = report(config, contract, wanted)
        write(
            config.destination("timing.json"),
            dict(seconds=time.perf_counter() - started),
        )
        return result

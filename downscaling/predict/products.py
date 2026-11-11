"""Request-derived GeoTIFFs and completion based on verified products."""

import json
import numpy as np
import pandas as pd
import rasterio
from rasterio.windows import Window
from downscaling.artifacts import read, write, receipt, valid, require
from downscaling.contracts import TARGETS
from downscaling.prepare.sources import Sources
from downscaling.prepare.domain import points_for_tile
from downscaling.predict.state import budget, verify_ledger

BANDS = (
    "fold1_mean",
    "fold2_mean",
    "fold3_mean",
    "ensemble_mean",
    "fold_std",
    "valid_steps",
    "expected_steps",
    "support_fraction",
    "max_neighbor_km",
    "missing_reason",
    "vi_filled_steps",
    "longest_missing_steps",
)


def inventory(config, wanted):
    windows = [
        ("period", str(r.period_id), [(r.start, r.stop)]) for r in wanted.itertuples()
    ]
    months = sorted(
        {
            t.strftime("%Y-%m")
            for r in wanted.itertuples()
            for t in pd.date_range(r.start, r.stop, freq="3h", inclusive="left")
        }
    )
    for month in months:
        begin = pd.Timestamp(month, tz="UTC")
        end = begin + pd.offsets.MonthBegin(1)
        segments = [
            (max(r.start, begin), min(r.stop, end))
            for r in wanted.itertuples()
            if r.stop > begin and r.start < end
        ]
        windows.append(("month", month, segments))
    return [
        dict(
            path=config.destination(f"products/{scale}/{target}_{key}.tif"),
            scale=scale,
            key=key,
            target=target,
            segments=[[str(a), str(b)] for a, b in segments],
            start=str(segments[0][0]),
            stop=str(segments[-1][1]),
            expected=sum(int((b - a).total_seconds() / 10800) for a, b in segments),
        )
        for scale, key, segments in windows
        for target in TARGETS
    ]


def profile(config):
    w = config.window
    with rasterio.open(config.sources["grid"]) as ref:
        return dict(
            driver="GTiff",
            height=w["height"],
            width=w["width"],
            crs=ref.crs,
            transform=ref.window_transform(
                Window(w["col"], w["row"], w["width"], w["height"])
            ),
            count=len(BANDS),
            dtype="float32",
            nodata=np.nan,
            tiled=True,
            blockxsize=256,
            blockysize=256,
            compress="deflate",
            predictor=3,
            BIGTIFF="IF_SAFER",
        )


def local_indices(pixel, tile, grid_width):
    rows = pixel // grid_width - tile["row"]
    cols = pixel % grid_width - tile["col"]
    if np.any(
        (rows < 0) | (rows >= tile["height"]) | (cols < 0) | (cols >= tile["width"])
    ):
        raise ValueError("Pixel outside declared block")
    return rows * tile["width"] + cols


def tile_values(config, tile, item, files, grid_width, mask, wanted):
    from downscaling.predict.engine import block_base

    n = tile["height"] * tile["width"]
    target_index = TARGETS.index(item["target"])
    sums = np.zeros((3, n))
    counts = np.zeros(n, np.uint32)
    distance = np.full(n, np.nan)
    reason = np.zeros(n, np.uint8)
    filled = np.zeros(n, np.uint32)
    longest = np.zeros(n, np.uint32)
    suffix = np.zeros(n, np.uint32)
    previous_stop = None
    for path in sorted(files, key=lambda p: int(p.stem.split("_")[1])):
        period_id = int(path.stem.split("_")[1])
        row = wanted[wanted.period_id == period_id].iloc[0]
        with np.load(path) as values:
            ix = local_indices(values["pixel"], tile, grid_width)
            if item["scale"] == "period":
                count = values["count"]
                total = values["fold_mean"][:, :, target_index] * count
                filled[ix] += values["vi_filled_steps"]
                longest[ix] = values["longest_missing_steps"]
            else:
                key = item["key"]
                if "count_" + key not in values:
                    continue
                start = max(row.start, pd.Timestamp(key, tz="UTC"))
                if previous_stop is not None and previous_stop != start:
                    suffix[:] = 0
                previous_stop = min(
                    row.stop, pd.Timestamp(key, tz="UTC") + pd.offsets.MonthBegin(1)
                )
                count = values["count_" + key]
                total = values["sum_" + key][:, :, target_index]
                filled[ix] += values["filled_" + key]
                longest[ix] = np.maximum.reduce(
                    [
                        longest[ix],
                        values["longest_" + key],
                        suffix[ix] + values["prefix_" + key],
                    ]
                )
                suffix[ix] = np.where(
                    values["suffix_" + key] == values["expected_" + key],
                    suffix[ix] + values["suffix_" + key],
                    values["suffix_" + key],
                )
            counts[ix] += count
            sums[:, ix] += np.nan_to_num(total)
            d = np.max(
                np.where(
                    np.isfinite(values["distance_km"]), values["distance_km"], -np.inf
                ),
                axis=0,
            )
            d[~np.isfinite(d)] = np.nan
            distance[ix] = np.fmax(distance[ix], d)
            reason[ix] |= values["reason"]
    excluded = pd.read_parquet(block_base(config, tile) / "excluded.parquet")[
        "index"
    ].to_numpy()
    reason[local_indices(excluded, tile, grid_width)] |= 4
    means = np.divide(
        sums, counts[None, :], out=np.full_like(sums, np.nan), where=counts[None, :] > 0
    )
    result = np.vstack(
        [
            means,
            means.mean(0),
            means.std(0),
            counts,
            np.full(n, item["expected"]),
            counts / item["expected"],
            distance,
            reason,
            filled,
            longest,
        ]
    ).astype(np.float32)
    land = mask.ravel()
    result[:, ~land] = np.nan
    if np.any(land & (counts == 0) & (reason == 0)):
        raise ValueError("Missing support without explicit reason")
    return result.reshape(len(BANDS), tile["height"], tile["width"])


def export(config, contract, wanted):
    from downscaling.predict.engine import verify_blocks, block_base

    by_period = verify_blocks(config, contract, wanted)
    token = contract["token"]
    sources = Sources(config.sources)
    raster_profile = profile(config)
    with rasterio.open(config.sources["grid"]) as grid:
        grid_width = grid.width
    for item in inventory(config, wanted):
        path = item["path"]
        if valid(path, token):
            continue
        budget(
            config,
            config.window["height"] * config.window["width"] * len(BANDS) * 4 + 65536,
        )
        path.parent.mkdir(parents=True, exist_ok=True)
        partial = path.with_suffix(".partial.tif")
        selected = (
            by_period[int(item["key"])]
            if item["scale"] == "period"
            else [p for paths in by_period.values() for p in paths]
        )
        with rasterio.open(partial, "w", **raster_profile) as output:
            for number, name in enumerate(BANDS, 1):
                output.set_band_description(number, name)
            output.update_tags(
                target=item["target"],
                scale=item["scale"],
                key=item["key"],
                start_utc=item["start"],
                stop_utc_exclusive=item["stop"],
                coverage_windows_utc=json.dumps(item["segments"]),
                expected_steps=str(item["expected"]),
                map_token=token,
                model_id=contract["model_id"],
                native_window="UTC [t,t+3h)",
                units="NEE model convention" if item["target"] == "NEE" else "W m-2",
                interpretation="Three-fold mean emulation; fold_std is not a prediction interval",
                missing_reason_bits="1:proxy ineligible;2:native category unsupported/missing;4:timezone missing",
            )
            for tile in contract["tiles"]:
                _, mask = points_for_tile(config, sources, tile)
                files = [p for p in selected if p.parent == block_base(config, tile)]
                values = tile_values(
                    config, tile, item, files, grid_width, mask, wanted
                )
                output.write(
                    values,
                    window=Window(
                        tile["col"] - config.window["col"],
                        tile["row"] - config.window["row"],
                        tile["width"],
                        tile["height"],
                    ),
                )
        partial.replace(path)
        receipt(path, token)
    verify_products(config, contract, wanted)


def verify_products(config, contract, wanted):
    items = inventory(config, wanted)
    expected_profile = profile(config)
    sources = Sources(config.sources)
    if not items:
        raise ValueError("Empty product request")
    for item in items:
        path = item["path"]
        require(path, contract["token"])
        with rasterio.open(path) as dataset:
            if (dataset.shape, dataset.transform, dataset.crs) != (
                (expected_profile["height"], expected_profile["width"]),
                expected_profile["transform"],
                expected_profile["crs"],
            ):
                raise ValueError("Product grid mismatch")
            if dataset.descriptions != BANDS or dataset.dtypes != ("float32",) * len(
                BANDS
            ):
                raise ValueError("Product band contract mismatch")
            tags = dataset.tags()
            expected_tags = dict(
                target=item["target"],
                scale=item["scale"],
                key=item["key"],
                map_token=contract["token"],
                model_id=contract["model_id"],
                start_utc=item["start"],
                stop_utc_exclusive=item["stop"],
                expected_steps=str(item["expected"]),
                coverage_windows_utc=json.dumps(item["segments"]),
            )
            if any(tags.get(k) != v for k, v in expected_tags.items()):
                raise ValueError("Product metadata/token mismatch")
            for tile in contract["tiles"]:
                _, mask = points_for_tile(config, sources, tile)
                window = Window(
                    tile["col"] - config.window["col"],
                    tile["row"] - config.window["row"],
                    tile["width"],
                    tile["height"],
                )
                values = dataset.read(window=window)
                means = values[:5, mask]
                count, expected, fraction = values[5:8, mask]
                reason = values[9, mask]
                if (
                    not np.isnan(values[:, ~mask]).all()
                    or not np.isfinite(count).all()
                    or np.any(count != np.floor(count))
                    or np.any((count < 0) | (count > item["expected"]))
                    or not np.all(expected == item["expected"])
                    or not np.allclose(
                        fraction, count / item["expected"], rtol=0, atol=1e-7
                    )
                    or not np.isin(reason, np.arange(8)).all()
                    or np.any((count == 0) & (reason == 0))
                    or not np.isfinite(means[:, count > 0]).all()
                    or not np.isnan(means[:, count == 0]).all()
                ):
                    raise ValueError("Invalid product support")
    return items


def report(config, contract, wanted):
    from downscaling.predict.engine import verify_blocks

    verify_blocks(config, contract, wanted)
    verify_ledger(contract["sources"])
    items = verify_products(config, contract, wanted)
    result = dict(
        status="complete",
        token=contract["token"],
        model_id=contract["model_id"],
        products=[str(item["path"].relative_to(config.output)) for item in items],
    )
    path = config.destination("COMPLETE.json")
    if path.exists():
        require(path, contract["token"])
        if read(path) != result:
            raise ValueError("Stale completion record")
    else:
        write(path, result)
        receipt(path, contract["token"])
    return result

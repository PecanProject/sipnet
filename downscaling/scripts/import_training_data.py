"""Copy receipt-verified prepared arrays into an independent training dataset.

The source cache is a data-only boundary. No source Python module is imported.
An explicit shard list is required, including for a bounded selection.
"""

import argparse
from pathlib import Path
import numpy as np
import pandas as pd
from downscaling.artifacts import digest, identity, read, require, sha, write
from downscaling.contracts import NUMERIC
from downscaling.train.data import NATIVE_ARRAYS, PANEL_ARRAYS, publish
from downscaling.train.splits import declare_nested, align_ids


def convert(source, source_manifest, shard_names, output, selection=None):
    source, output = Path(source).resolve(), Path(output).resolve()
    if output.exists() and any(output.iterdir()):
        raise ValueError("Choose an empty prepared-data output directory")
    if output.is_relative_to(source) or source.is_relative_to(output):
        raise ValueError("Prepared source and destination must be disjoint")
    manifest = read(source_manifest)
    payload = {key: value for key, value in manifest.items() if key != "token"}
    if digest(payload) != manifest["token"]:
        raise ValueError("Source manifest digest mismatch")
    from downscaling.train.data import required_files

    required_files(shard_names)
    paths = [
        source / "cache" / name for name in ("folds.csv", "periods.csv", "panel.json")
    ]
    paths += [source / f"cache/{key}.npy" for key in PANEL_ARRAYS]
    paths += [
        source / f"cache/native/{name}.{key}.npy"
        for name in shard_names
        for key in NATIVE_ARRAYS
    ]
    paths += [source / f"cache/native/{name}.json" for name in shard_names]
    # All declared sources and receipts must exist before any destination write.
    for path in paths:
        require(path, manifest["token"])
    panel = read(source / "cache/panel.json")
    if panel["numeric_names"] != NUMERIC:
        raise ValueError("Prepared proxy feature order mismatch")
    folds = pd.read_csv(source / "cache/folds.csv")
    arrays = {
        key: np.load(source / f"cache/{key}.npy", mmap_mode="r") for key in PANEL_ARRAYS
    }
    folds = folds.iloc[
        align_ids(folds["index"], arrays["site_ids"], "source panel sites")
    ].copy()
    folds = declare_nested(folds)
    periods = pd.read_csv(source / "cache/periods.csv")
    n, p = len(folds), len(periods)
    complete = np.array(arrays["complete"], copy=True).reshape(p, n)
    selected_sites = np.arange(n)
    stride = 1
    if selection is not None:
        if set(selection) != {"site_ids", "period_ids", "native_day_stride"}:
            raise ValueError(
                "Selection requires site_ids, period_ids, native_day_stride"
            )
        selected_sites = align_ids(
            arrays["site_ids"], selection["site_ids"], "selected sites"
        )
        selected_sites.sort()
        positions = align_ids(
            periods.period_id, selection["period_ids"], "selected periods"
        )
        mask = np.zeros_like(complete)
        mask[np.ix_(positions, selected_sites)] = True
        complete &= mask
        stride = selection["native_day_stride"]
        if type(stride) is not int or stride < 1:
            raise ValueError("native_day_stride must be positive")
    site_map = np.full(n, -1, np.int64)
    site_map[selected_sites] = np.arange(len(selected_sites))
    output.mkdir(parents=True, exist_ok=True)
    (output / "panel").mkdir()
    (output / "native").mkdir()
    folds.iloc[selected_sites].to_csv(output / "folds.csv", index=False)
    periods.to_csv(output / "periods.csv", index=False)
    for key in PANEL_ARRAYS:
        if key == "site_ids":
            value = arrays[key][selected_sites]
        elif key == "complete":
            value = complete[:, selected_sites].ravel()
        else:
            tail = arrays[key].shape[1:]
            value = (
                arrays[key].reshape(p, n, *tail)[:, selected_sites].reshape(-1, *tail)
            )
        np.save(output / f"panel/{key}.npy", value, allow_pickle=False)
    names = []
    rows = 0
    for name in shard_names:
        metadata = read(source / f"cache/native/{name}.json")
        if metadata["name"] != name:
            raise ValueError(f"Source shard identity mismatch: {name}")
        row = np.load(source / f"cache/native/{name}.row.npy", mmap_mode="r")
        step = np.load(source / f"cache/native/{name}.step.npy", mmap_mode="r")
        if len(row) != metadata["rows"] or len(step) != len(row):
            raise ValueError(f"Source shard length mismatch: {name}")
        take = np.flatnonzero(complete.ravel()[row] & ((step // 8) % stride == 0))
        if selection is not None and not len(take):
            continue
        names.append(name)
        for key in NATIVE_ARRAYS:
            source_array = np.load(
                source / f"cache/native/{name}.{key}.npy", mmap_mode="r"
            )
            if len(source_array) != len(row):
                raise ValueError(f"Source shard length mismatch: {name}.{key}")
            value = source_array[take]
            if key == "row":
                value = (
                    (value // n) * len(selected_sites) + site_map[value % n]
                ).astype(np.int32)
            np.save(output / f"native/{name}.{key}.npy", value, allow_pickle=False)
        write(output / f"native/{name}.json", dict(name=name, rows=len(take)))
        rows += len(take)
    provenance = dict(
        source_token=manifest["token"],
        source_manifest_sha256=sha(source_manifest),
        source_files=[
            dict(artifact=identity(path), receipt_sha256=sha(str(path) + ".ok.json"))
            for path in paths
        ],
        source_shards=shard_names,
        selection=selection,
        nested_membership="assigned on full source inventory before subsetting",
    )
    result = publish(output, names, provenance, str(periods.start_utc.iloc[0]))
    print(
        dict(
            dataset=result["id"],
            sites=len(selected_sites),
            proxy_rows=int(complete.sum()),
            native_rows=rows,
            shards=len(names),
        ),
        flush=True,
    )
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--source-manifest", type=Path, required=True)
    parser.add_argument(
        "--shards",
        type=Path,
        required=True,
        help="JSON list of all expected source shard IDs",
    )
    parser.add_argument(
        "--selection", type=Path, help="Optional explicit bounded selection JSON"
    )
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    convert(
        args.source,
        args.source_manifest,
        read(args.shards),
        args.output,
        None if args.selection is None else read(args.selection),
    )


if __name__ == "__main__":
    main()

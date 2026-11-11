"""Run a declared reference window, compare products, and verify cache reuse."""

import argparse
import resource
import sys
import time
from pathlib import Path
import numpy as np
import pandas as pd
import rasterio
from downscaling.configuration import load
from downscaling.artifacts import read, sha, write, identity
from downscaling.predict.engine import execute
from downscaling.models.inference import Models, native_matrix
from downscaling.contracts import predictors


def models_match(config, reference):
    models = Models(config.model_bundle, config.device, config.cpu_threads)
    frame = pd.read_parquet(reference / "proxy.parquet")
    with np.load(reference / "proxy.npz") as expected:
        numeric, lc, valid = predictors(frame)
        for actual, key in [(numeric, "numeric"), (lc, "lc"), (valid, "valid")]:
            np.testing.assert_array_equal(actual, expected[key])
        actual, valid = models.proxy(frame)
        np.testing.assert_allclose(actual, expected["prediction"], rtol=1e-5, atol=2e-5)
    for year in (2012, 2018, 2024):
        frame = pd.read_parquet(reference / f"native_{year}.parquet")
        with np.load(reference / f"native_{year}.npz") as expected:
            np.testing.assert_array_equal(native_matrix(frame), expected["matrix"])
            actual = models.native(
                frame, np.zeros((3, len(frame), 2)), np.zeros((3, len(frame), 96, 2))
            )
            np.testing.assert_allclose(
                actual, -expected["residual"], rtol=1e-6, atol=1e-5
            )
    for fold in (1, 2, 3):
        frame = pd.read_parquet(reference / f"spatial_{fold}.parquet")
        bias, distance = models.spatial[fold].query(frame, frame.LC.to_numpy())
        with np.load(reference / f"spatial_{fold}.npz") as expected:
            np.testing.assert_allclose(bias, expected["bias"], rtol=1e-12, atol=1e-12)
            np.testing.assert_allclose(
                distance, expected["distance"], rtol=1e-12, atol=1e-12
            )


def validate(config, reference):
    manifest = read(reference / "manifest.json")
    if (
        config.window != manifest["window"]
        or list(config.period_ids) != manifest["period_ids"]
    ):
        raise ValueError("Validation request differs from reference window")
    for name, expected in manifest["files"].items():
        if sha(reference / name) != expected:
            raise ValueError(f"Changed reference: {name}")
    started = time.perf_counter()
    models_match(config, reference)
    model_seconds = time.perf_counter() - started
    started = time.perf_counter()
    result = execute("run", config, resume=config.destination("manifest.json").exists())
    run_seconds = time.perf_counter() - started
    w = config.window
    base = config.destination(f"blocks/r{w['row']}_c{w['col']}")
    points = pd.read_parquet(base / "points.parquet")
    pd.testing.assert_frame_equal(
        points, pd.read_parquet(reference / "points.parquet"), check_dtype=False
    )
    for period_id in range(min(config.period_ids) - 4, max(config.period_ids) + 1):
        actual = pd.read_parquet(base / f"input_{period_id:03d}.parquet")
        expected = pd.read_parquet(reference / f"input_{period_id}.parquet")
        np.testing.assert_array_equal(actual["index"], expected["index"])
        for column in expected:
            if pd.api.types.is_numeric_dtype(expected[column]):
                np.testing.assert_array_equal(
                    actual[column].to_numpy(np.float32),
                    expected[column].to_numpy(np.float32),
                    err_msg=column,
                )
            else:
                np.testing.assert_array_equal(
                    actual[column], expected[column], err_msg=column
                )
    maximum = 0.0
    for period_id in config.period_ids:
        with (
            np.load(base / f"period_{period_id:03d}.npz") as actual,
            np.load(reference / f"period_{period_id}.npz") as expected,
        ):
            assert set(actual.files) == set(expected.files)
            for name in expected.files:
                if name == "fold_mean" or name.startswith("sum_"):
                    np.testing.assert_allclose(
                        actual[name], expected[name], rtol=1e-6, atol=1e-5, err_msg=name
                    )
                    maximum = max(
                        maximum, float(np.nanmax(abs(actual[name] - expected[name])))
                    )
                else:
                    np.testing.assert_array_equal(
                        actual[name], expected[name], err_msg=name
                    )
        with np.load(reference / f"period_{period_id}.npz") as expected:
            for target_index, target in enumerate(("NEE", "LE")):
                with rasterio.open(
                    config.destination(f"products/period/{target}_{period_id}.tif")
                ) as raster:
                    geo = read(reference / "grid.json")
                    assert (
                        list(raster.transform) == geo["transform"]
                        and str(raster.crs) == geo["crs"]
                        and list(raster.shape) == geo["shape"]
                    )
                    np.testing.assert_allclose(
                        raster.read([1, 2, 3]).reshape(3, -1),
                        expected["fold_mean"][:, :, target_index],
                        rtol=1e-6,
                        atol=1e-5,
                    )
                    np.testing.assert_array_equal(
                        raster.read(6).ravel(), expected["count"]
                    )
    artifacts = [
        p
        for p in config.output.rglob("*")
        if p.is_file()
        and (
            p.suffix in (".npz", ".tif", ".parquet", ".npy")
            or p.name == "COMPLETE.json"
        )
    ]
    before = {str(p): identity(p) for p in artifacts}
    started = time.perf_counter()
    assert execute("run", config, resume=True) == result
    resume_seconds = time.perf_counter() - started
    assert before == {str(p): identity(p) for p in artifacts}
    module_paths = sorted(
        {
            str(Path(module.__file__).resolve())
            for module in sys.modules.values()
            if getattr(module, "__file__", None) and Path(module.__file__).is_file()
        }
    )
    allowed = (
        Path(sys.prefix).resolve(),
        Path(sys.base_prefix).resolve(),
        Path(__file__).resolve().parents[1],
    )
    if any(
        not any(Path(path).is_relative_to(root) for root in allowed)
        for path in module_paths
    ):
        raise AssertionError("Unexpected source-project module dependency")
    record = dict(
        status="passed",
        device=config.device,
        model_replay_seconds=model_seconds,
        run_seconds=read(config.destination("timing.json"))["seconds"],
        validation_invocation_seconds=run_seconds,
        resume_seconds=resume_seconds,
        maximum_block_absolute_difference=maximum,
        peak_rss_kib=resource.getrusage(resource.RUSAGE_SELF).ru_maxrss,
        worker_peak_rss_kib=resource.getrusage(resource.RUSAGE_CHILDREN).ru_maxrss,
        points=len(points),
        period_ids=list(config.period_ids),
        products=result["products"],
        python=sys.executable,
        module_paths=module_paths,
    )
    write(config.destination("validation.json"), record)
    return record


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--config", required=True)
    parser.add_argument("--reference", required=True)
    args = parser.parse_args()
    record = validate(load(args.config), Path(args.reference))
    print({key: value for key, value in record.items() if key != "module_paths"})


if __name__ == "__main__":
    main()

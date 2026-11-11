"""Replay an exported training bundle through the deployed inference interfaces."""

import argparse
from pathlib import Path
from types import SimpleNamespace
import resource
import time
import numpy as np
import pandas as pd
import torch
from downscaling.artifacts import identity, write
from downscaling.contracts import KGC, NATIVE, predictors
from downscaling.models.inference import Models, native_matrix
from downscaling.train.configuration import load
from downscaling.train.state import Run
from downscaling.train.engine import execute, stages
from downscaling.train import proxy, correct
from downscaling.scripts.validate import loaded_module_paths


def replay(run, proxy_frame):
    models = Models(
        run.root / "model_bundle", run.config.device, run.config.cpu_threads
    )
    raw, lc, eligible = predictors(proxy_frame)
    actual, valid = models.proxy(proxy_frame)
    np.testing.assert_array_equal(valid, eligible)
    years = pd.to_datetime(proxy_frame.period_key).dt.year.to_numpy()
    proxy_max = 0.0
    for outer in (1, 2, 3):
        model, checkpoint = proxy.load_model(run, proxy.name(outer))
        metadata = checkpoint["metadata"]
        pre = metadata["preprocessing"]
        prepared = SimpleNamespace(
            numeric=(
                (raw - np.asarray(pre["numeric_mean"])) / np.asarray(pre["numeric_std"])
            ).astype(np.float32),
            lc=np.nan_to_num(lc, nan=0).astype(np.int64),
            year_ids=np.array(
                [
                    (
                        int(year) - 2012
                        if year in pre["fit_years"] and 2013 <= year <= 2024
                        else 0
                    )
                    for year in years
                ],
                np.int64,
            ),
            target_center=np.asarray(metadata["target_center"]),
            target_scale=np.asarray(metadata["target_scale"]),
        )
        expected = proxy.infer(model, prepared, np.flatnonzero(eligible))[:, :, 0]
        np.testing.assert_allclose(
            actual[outer - 1, eligible], expected, rtol=1e-5, atol=2e-5
        )
        proxy_max = max(
            proxy_max, float(np.abs(actual[outer - 1, eligible] - expected).max())
        )
    native_rows, native_max, spatial_max = 0, 0.0, 0.0
    for outer in (1, 2, 3):
        for shard in run.data.shards:
            offsets = correct.selected(run.data, shard, outer, 0, False)[:128]
            if not len(offsets):
                continue
            rows = run.data.native(shard, "row")[offsets]
            steps = run.data.native(shard, "step")[offsets]
            values = run.data.native(shard, "x")[offsets]
            frame = pd.DataFrame(values[:, :31], columns=NATIVE)
            frame["KGC"] = [KGC[int(value)] for value in values[:, 31]]
            frame["LC"] = values[:, 32].astype(int)
            frame["utc"] = run.data.times(steps)
            np.testing.assert_array_equal(
                native_matrix(frame), run.data.matrix(shard, offsets)
            )
            points = run.data.folds.iloc[rows % run.data.n]
            bias, distance = models.spatial[outer].query(points, points.LC.to_numpy())
            spatial = run.load(f"models/spatial_o{outer}.npz")
            expected_bias = np.clip(
                spatial["bias"][rows % run.data.n], -spatial["caps"], spatial["caps"]
            )
            np.testing.assert_allclose(
                bias, expected_bias, rtol=1e-12, atol=1e-12, equal_nan=True
            )
            np.testing.assert_allclose(
                distance,
                spatial["distance_km"][rows % run.data.n],
                rtol=1e-12,
                atol=1e-12,
                equal_nan=True,
            )
            finite = np.isfinite(bias) & np.isfinite(expected_bias)
            spatial_max = max(
                spatial_max,
                float(np.abs(bias[finite] - expected_bias[finite]).max(initial=0)),
            )
            # Other fold outputs are irrelevant to this fold's held-out comparison.
            biases = np.zeros((3, len(offsets), 96, 2))
            biases[outer - 1] = bias
            proxy_values = np.zeros((3, len(offsets), 2), np.float32)
            proxy_values[outer - 1] = run.array(proxy.assembled_path(outer))[rows, :, 0]
            actual_native = models.native(frame, proxy_values, biases)[outer - 1]
            expected_native = run.load(f'predictions/final/{shard["name"]}.npz')[
                "location"
            ][offsets]
            np.testing.assert_allclose(
                actual_native, expected_native, rtol=1e-6, atol=1e-5
            )
            native_max = max(
                native_max, float(np.abs(actual_native - expected_native).max())
            )
            native_rows += len(offsets)
            break
        else:
            raise ValueError(f"No native replay samples for fold {outer}")
    return dict(
        proxy_rows=int(eligible.sum()),
        native_rows=native_rows,
        proxy_max_abs=proxy_max,
        native_max_abs=native_max,
        spatial_max_abs=spatial_max,
    )


def validate(config, proxy_fixture, output):
    start = time.perf_counter()
    result = execute("report", config)
    if result["status"] != "complete":
        raise ValueError("Training is incomplete")
    run = Run(config)
    replay_start = time.perf_counter()
    comparisons = replay(run, pd.read_parquet(proxy_fixture))
    replay_seconds = time.perf_counter() - replay_start
    files = sorted(
        {path for _, outputs, _, _ in stages(run.data) for path in outputs}
        | {"COMPLETE.json"}
    )
    before = {path: identity(run.root / path) for path in files}
    resume_start = time.perf_counter()
    resumed = execute("run", config, resume=True)
    after = {path: identity(run.root / path) for path in files}
    if before != after or resumed != result:
        raise ValueError("Resume rewrote completed training artifacts")
    import sys

    roots = [
        Path(sys.prefix).resolve(),
        Path(sys.base_prefix).resolve(),
        Path(__file__).resolve().parents[2],
    ]
    modules = loaded_module_paths()
    unexpected = [
        path
        for path in modules
        if not any(Path(path).is_relative_to(root) for root in roots)
    ]
    if unexpected:
        raise ValueError(f"Unexpected runtime module sources: {unexpected}")
    evidence = dict(
        status="passed",
        device=config.device,
        training_token=run.token,
        model_id=result["model_id"],
        comparisons=comparisons,
        completed_artifacts=len(files),
        resume_preserved_identities=True,
        replay_seconds=replay_seconds,
        resume_seconds=time.perf_counter() - resume_start,
        total_seconds=time.perf_counter() - start,
        peak_rss_kib=resource.getrusage(resource.RUSAGE_SELF).ru_maxrss,
        torch=torch.__version__,
        gpu=torch.cuda.get_device_name(0) if config.device == "cuda" else None,
        module_paths=modules,
    )
    write(output, evidence)
    return evidence


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--config", required=True)
    parser.add_argument(
        "--proxy-fixture",
        type=Path,
        required=True,
        help="Prepared raw predictor table in Parquet format",
    )
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    result = validate(load(args.config), args.proxy_fixture, args.output)
    print({key: value for key, value in result.items() if key != "module_paths"})


if __name__ == "__main__":
    main()

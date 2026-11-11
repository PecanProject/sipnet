"""A sequential, locked training pipeline with explicit stage output contracts."""

from pathlib import Path
import fcntl
import os
import time
from downscaling.artifacts import read, require, write
from downscaling.models.bundle import validate as validate_bundle
from downscaling.train.state import Run
from downscaling.train.splits import proxy_keys
from downscaling.train import proxy, correct, evaluate, bundle


def stages(data):
    result = []
    for outer in (1, 2, 3):
        for inner in (0, 1, 2, 3):
            result.append(
                (
                    proxy.name(outer, inner),
                    proxy.outputs(outer, inner),
                    "proxy",
                    (outer, inner, 0),
                )
            )
        result.append(
            (
                f"proxy_assemble_{outer}_0",
                [proxy.assembled_path(outer)],
                "proxy_assemble",
                (outer, 0),
            )
        )
        for target in (0, 1):
            result.append(
                (
                    correct.name("mean", outer, 0, target),
                    correct.outputs(data, "mean", outer, 0, target),
                    "correct",
                    ("mean", outer, 0, target),
                )
            )
        for inner in (1, 2, 3):
            for nested in (1, 2, 3):
                result.append(
                    (
                        proxy.name(outer, inner, nested),
                        proxy.outputs(outer, inner, nested),
                        "proxy",
                        (outer, inner, nested),
                    )
                )
            result.append(
                (
                    f"proxy_assemble_{outer}_{inner}",
                    [proxy.assembled_path(outer, inner)],
                    "proxy_assemble",
                    (outer, inner),
                )
            )
            for target in (0, 1):
                result.append(
                    (
                        correct.name("mean", outer, inner, target),
                        correct.outputs(data, "mean", outer, inner, target),
                        "correct",
                        ("mean", outer, inner, target),
                    )
                )
        result.append(
            (f"nested_{outer}", correct.nested_outputs(data, outer), "nested", (outer,))
        )
        for target in (0, 1):
            result.append(
                (
                    correct.name("quantile", outer, 0, target),
                    correct.outputs(data, "quantile", outer, 0, target),
                    "correct",
                    ("quantile", outer, 0, target),
                )
            )
        result.append(
            (
                f"spatial_{outer}",
                [f"models/spatial_o{outer}.npz", f"validation/spatial_o{outer}.json"],
                "spatial",
                (outer,),
            )
        )
    result += [
        ("report", evaluate.report_outputs(data), "report", ()),
        ("bundle", bundle.outputs(), "bundle", ()),
    ]
    return result


def status(run, expected):
    completed = []
    for name, outputs, _, _ in expected:
        marker = run.root / f"stages/{name}.json"
        if marker.exists() or Path(str(marker) + ".ok.json").exists():
            run.verify_stage(name, outputs)
            completed.append(name)
    complete = run.root / "COMPLETE.json"
    if complete.exists() or Path(str(complete) + ".ok.json").exists():
        require(complete, run.token)
        record = read(complete)
        if (
            len(completed) != len(expected)
            or record.get("stages") != completed
            or record.get("token") != run.token
            or record.get("status") != "complete"
        ):
            raise ValueError("Completion record does not match verified stages")
        model = validate_bundle(run.root / "model_bundle")
        if record.get("model_id") != model["id"]:
            raise ValueError("Completion/model bundle identity mismatch")
        return record
    return dict(
        status="incomplete",
        token=run.token,
        completed_stages=len(completed),
        expected_stages=len(expected),
        remaining=[name for name, *_ in expected if name not in completed],
    )


def execute(command, config, resume=False):
    run = Run(config)
    # Validate every epoch-selection split before committing to any training work.
    for key in proxy_keys():
        proxy.training_rows(run.data, *key)
    expected = stages(run.data)
    if command == "report":
        if (
            not (run.root / "manifest.json").exists()
            or read(run.root / "manifest.json") != run.manifest
        ):
            raise ValueError("Missing or changed training request")
        return status(run, expected)
    if command not in ("preflight", "run"):
        raise ValueError("Unknown training command")
    slots = int(os.environ.get("NSLOTS", config.cpu_threads))
    if config.cpu_threads > slots:
        raise ValueError("Requested CPU threads exceed the assigned slots")
    if config.device == "cuda":
        import torch
        import cupy as cp

        if (
            not torch.cuda.is_available()
            or torch.cuda.device_count() != 1
            or cp.cuda.runtime.getDeviceCount() != 1
        ):
            raise RuntimeError(
                "CUDA requested; exactly one allocated visible GPU is required"
            )
    run.root.mkdir(parents=True, exist_ok=True)
    with (run.root / "run.lock").open("a") as lock:
        try:
            fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError as error:
            raise RuntimeError("Another process owns this training run") from error
        run.freeze()
        current = status(run, expected)
        if command == "preflight":
            return dict(
                status="passed",
                token=run.token,
                sites=run.data.n,
                proxy_rows=int(run.data.arrays["complete"].sum()),
                native_rows=sum(shard["rows"] for shard in run.data.shards),
                splits=39,
                stages=len(expected),
                device=config.device,
            )
        if not resume and any(
            (run.root / directory).exists()
            for directory in ("models", "cache", "stages")
        ):
            raise ValueError("Existing training artifacts require run --resume")
        if current["status"] == "complete":
            return current
        workers = dict(
            proxy=proxy.train,
            proxy_assemble=proxy.assemble,
            correct=correct.fit,
            nested=correct.assemble,
            spatial=evaluate.spatial,
            report=evaluate.report,
            bundle=bundle.export,
        )
        start = time.perf_counter()
        for name, outputs, task, args in expected:
            try:
                run.stage(
                    name,
                    outputs,
                    lambda task=task, args=args: workers[task](run, *args),
                )
            except Exception as error:
                write(
                    run.root / "status.json",
                    dict(
                        status="failed", stage=name, error=str(error), token=run.token
                    ),
                )
                raise
        for name, outputs, _, _ in expected:
            run.verify_stage(name, outputs)
        model = validate_bundle(run.root / "model_bundle")
        run.json(
            "COMPLETE.json",
            dict(
                status="complete",
                token=run.token,
                model_id=model["id"],
                stages=[stage[0] for stage in expected],
                targets=["NEE", "LE"],
                folds=3,
                seconds_this_invocation=time.perf_counter() - start,
            ),
        )
        return status(run, expected)

"""External-memory mean correction and strictly nested Q90 interval calibration."""

import gc
import time
import numpy as np
import xgboost as xgb
from downscaling.artifacts import receipt
from downscaling.contracts import TARGETS
from downscaling.train.recipe import BOOSTING, ROUNDS, Q90, EPS
from downscaling.train.proxy import assembled_path


def name(mode, outer, inner, target):
    if mode not in ("mean", "quantile") or (mode == "quantile" and inner):
        raise ValueError("Invalid correction request")
    return f"{mode}_o{outer}_j{inner}_{TARGETS[target]}"


def selected(data, shard, outer, inner=0, train=True):
    sites = data.native(shard, "row") % data.n
    outer_fold = data.folds.fold.to_numpy()[sites]
    inner_fold = data.folds[f"inner_{outer}"].to_numpy()[sites]
    if inner:
        mask = (outer_fold != outer) & (
            (inner_fold != inner) if train else (inner_fold == inner)
        )
    else:
        mask = (outer_fold != outer) if train else (outer_fold == outer)
    return np.flatnonzero(mask)


def outputs(data, mode, outer, inner, target):
    key = name(mode, outer, inner, target)
    return [
        f"models/{key}.json",
        f"validation/{key}.json",
        *[
            f'predictions/{key}/{shard["name"]}.npz'
            for shard in data.shards
            if len(selected(data, shard, outer, inner, False))
        ],
    ]


def features(data, shard, offsets, proxy_location, corrected_location, scale):
    """Float32 native features plus proxy bounds, correction, mean and width."""
    width = Q90 * scale
    extra = np.column_stack(
        [
            proxy_location,
            proxy_location - width,
            proxy_location + width,
            corrected_location - proxy_location,
            corrected_location,
            width,
        ]
    ).astype(np.float32)
    return np.concatenate([data.matrix(shard, offsets), extra], axis=1)


def training_data(run, mode, shard, outer, inner, target, proxy):
    data = run.data
    offsets = selected(data, shard, outer, inner, True)
    rows = data.native(shard, "row")[offsets]
    if mode == "mean":
        matrix = data.matrix(shard, offsets)
        labels = proxy[rows, target, 0] - data.native(shard, "y")[offsets, target]
    else:
        nested = run.load(f'cache/nested_o{outer}/{shard["name"]}.npz')
        np.testing.assert_array_equal(offsets, nested["offset"])
        matrix = features(
            data,
            shard,
            offsets,
            proxy[rows, target, 0],
            nested["mu"][:, target],
            proxy[rows, target, 1],
        )
        labels = nested["logk"][:, target]
    if not np.isfinite(labels).all():
        raise ValueError(f'Nonfinite calibration labels: {mode}/{shard["name"]}')
    return offsets, matrix, labels


def inference_data(run, mode, shard, outer, inner, target, proxy):
    """Prediction boundary reads features and upstream predictions, never y/route."""
    data = run.data
    offsets = selected(data, shard, outer, inner, False)
    rows = data.native(shard, "row")[offsets]
    location, scale = proxy[rows, target, 0], proxy[rows, target, 1]
    if mode == "mean":
        return offsets, data.matrix(shard, offsets), location, scale
    if not len(offsets):
        return (
            offsets,
            features(data, shard, offsets, location, location, scale),
            location,
            scale,
        )
    mean = run.load(f'predictions/{name("mean", outer, 0, target)}/{shard["name"]}.npz')
    np.testing.assert_array_equal(offsets, mean["offset"])
    corrected = mean["mu"]
    return (
        offsets,
        features(data, shard, offsets, location, corrected, scale),
        corrected,
        scale,
    )


class Iterator(xgb.DataIter):
    def __init__(self, run, mode, outer, inner, target):
        key = name(mode, outer, inner, target)
        super().__init__(
            cache_prefix=str(run.path(f"tmp/{key}")), on_host=True, release_data=True
        )
        self.run, self.mode, self.outer, self.inner, self.target = (
            run,
            mode,
            outer,
            inner,
            target,
        )
        self.proxy_predictions = run.array(assembled_path(outer, inner if mode == "mean" else 0))
        self.gpu = run.config.device == "cuda"
        self.reset()

    def reset(self):
        self.position = 0

    def next(self, input_data):
        while self.position < len(self.run.data.shards):
            shard = self.run.data.shards[self.position]
            self.position += 1
            offsets, matrix, labels = training_data(
                self.run,
                self.mode,
                shard,
                self.outer,
                self.inner,
                self.target,
                self.proxy_predictions,
            )
            if not len(offsets):
                continue
            if self.gpu:
                import cupy as cp

                matrix, labels = cp.asarray(matrix), cp.asarray(
                    labels, dtype=cp.float32
                )
            self.matrix, self.labels = matrix, labels
            input_data(data=self.matrix, label=self.labels)
            return True
        return False


class Progress(xgb.callback.TrainingCallback):
    def __init__(self, run, key):
        self.run, self.key = run, key
        self.start = time.perf_counter()

    def after_iteration(self, model, epoch, evals_log):
        rounds = epoch + 1
        if rounds % 10 == 0:
            save_model(self.run, self.key, model, final=False)
            self.run.json(
                f"validation/{self.key}_progress.json",
                dict(
                    rounds=rounds, seconds_since_resume=time.perf_counter() - self.start
                ),
            )
        return False


def save_model(run, key, model, final):
    path = run.path(f"models/{key}.json")
    partial = path.with_suffix(".partial.json")
    model.set_attr(training_token=run.token)
    model.save_model(partial)
    partial.replace(path)
    if final:
        receipt(path, run.token)


def predict(model, matrix, device):
    if device == "cuda":
        import cupy as cp

        return model.inplace_predict(cp.asarray(matrix)).get()
    return model.inplace_predict(matrix)


def fit(run, mode, outer, inner, target):
    key = name(mode, outer, inner, target)
    path = run.path(f"models/{key}.json")
    start = time.perf_counter()
    pool = None
    gpu = run.config.device == "cuda"
    if gpu:
        import cupy as cp

        if cp.cuda.runtime.getDeviceCount() != 1:
            raise RuntimeError("Exactly one visible CUDA device required")
        xgb.set_config(use_cuda_async_pool=True)
        pool = cp.cuda.MemoryAsyncPool()
        cp.cuda.set_allocator(pool.malloc)
    iterator = Iterator(run, mode, outer, inner, target)
    options = dict(max_quantile_batches=8, cache_host_ratio=None) if gpu else {}
    matrix = xgb.ExtMemQuantileDMatrix(
        iterator, max_bin=256, nthread=run.config.cpu_threads, **options
    )
    built = time.perf_counter() - start
    if not matrix.num_row():
        raise ValueError(f"Empty calibration training set: {key}")
    params = dict(
        BOOSTING,
        device=run.config.device,
        nthread=run.config.cpu_threads,
        objective="reg:squarederror" if mode == "mean" else "reg:quantileerror",
    )
    if mode == "quantile":
        params["quantile_alpha"] = 0.9
    restarted_rounds = 0
    if path.exists():
        previous = xgb.Booster()
        previous.load_model(path)
        if previous.attr("training_token") != run.token:
            raise ValueError(f"Unbound partial calibration model: {key}")
        restarted_rounds = previous.num_boosted_rounds()
        if restarted_rounds > ROUNDS:
            raise ValueError("Partial model exceeds the fixed round count")
        del previous
    fit_start = time.perf_counter()
    # Saved trees do not preserve the sampling RNG state. Restart unfinished
    # stages with the original seed; Run.stage skips verified completed stages.
    model = xgb.train(
        params,
        matrix,
        num_boost_round=ROUNDS,
        callbacks=[Progress(run, key)],
    )
    save_model(run, key, model, final=True)
    fit_seconds = time.perf_counter() - fit_start
    del matrix, iterator
    gc.collect()
    if pool is not None:
        pool.free_all_blocks()
    reloaded = xgb.Booster()
    reloaded.load_model(path)
    reloaded.set_param(dict(device=run.config.device, nthread=run.config.cpu_threads))
    proxy = run.array(assembled_path(outer))
    count, checked = 0, False
    for shard in run.data.shards:
        offsets, matrix, location, scale = inference_data(
            run, mode, shard, outer, inner, target, proxy
        )
        if not len(offsets):
            continue
        prediction = predict(reloaded, matrix, run.config.device)
        if not np.isfinite(prediction).all():
            raise ValueError("Nonfinite calibration prediction")
        if not checked:
            np.testing.assert_array_equal(
                predict(model, matrix[:128], run.config.device), prediction[:128]
            )
            checked = True
        destination = f'predictions/{key}/{shard["name"]}.npz'
        if mode == "mean":
            run.save(
                destination,
                offset=offsets,
                mu=(location - prediction).astype(np.float32),
                scale=scale,
            )
        else:
            multiplier = np.exp(np.maximum(prediction, 0))
            if not np.isfinite(multiplier).all() or np.any(multiplier < 1):
                raise ValueError("Invalid interval multiplier")
            run.save(destination, offset=offsets, k=multiplier.astype(np.float32))
        count += len(offsets)
    if not checked:
        raise ValueError(f"Empty calibration prediction set: {key}")
    run.json(
        f"validation/{key}.json",
        dict(
            status="passed",
            mode=mode,
            outer=outer,
            inner=inner,
            target=TARGETS[target],
            params=params,
            rounds=ROUNDS,
            restarted_from_rounds=restarted_rounds,
            matrix_seconds=built,
            training_seconds=fit_seconds,
            total_seconds=time.perf_counter() - start,
            prediction_rows=count,
            checkpoint_reload=True,
            all_routes_retained=True,
        ),
    )


def nested_outputs(data, outer):
    return [
        f"validation/nested_o{outer}.json",
        *[f'cache/nested_o{outer}/{shard["name"]}.npz' for shard in data.shards],
    ]


def assemble(run, outer):
    total = 0
    data = run.data
    proxy = run.array(assembled_path(outer))
    for shard in data.shards:
        offsets = selected(data, shard, outer, 0, True)
        rows = data.native(shard, "row")[offsets]
        membership = data.folds[f"inner_{outer}"].to_numpy()[rows % data.n]
        means = np.full((len(offsets), 2), np.nan, np.float32)
        for inner in (1, 2, 3):
            keep = membership == inner
            if not keep.any():
                continue
            for target in range(2):
                values = run.load(
                    f'predictions/{name("mean", outer, inner, target)}/{shard["name"]}.npz'
                )
                np.testing.assert_array_equal(offsets[keep], values["offset"])
                means[keep, target] = values["mu"]
        if not np.isfinite(means).all():
            raise ValueError("Incomplete nested mean predictions")
        width = Q90 * proxy[rows, :, 1]
        truth = data.native(shard, "y")[offsets]
        multiplier = np.maximum(
            1,
            np.maximum(
                (means - truth) / (width + EPS), (truth - means) / (width + EPS)
            ),
        )
        if not np.isfinite(multiplier).all():
            raise ValueError("Invalid nested interval labels")
        run.save(
            f'cache/nested_o{outer}/{shard["name"]}.npz',
            offset=offsets,
            mu=means,
            logk=np.log(multiplier).astype(np.float32),
        )
        total += len(offsets)
    run.json(
        f"validation/nested_o{outer}.json",
        dict(
            status="passed", rows=total, strictly_nested=True, all_routes_retained=True
        ),
    )

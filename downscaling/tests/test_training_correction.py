import numpy as np
import pytest
import xgboost as xgb
from downscaling.artifacts import read
from downscaling.train.configuration import Config
from downscaling.train.state import Run
from downscaling.train import correct
from downscaling.train.proxy import assembled_path
from downscaling.train.recipe import Q90, EPS


def calibration_run(training_data, tmp_path):
    run = Run(Config(training_data, tmp_path / "run"))
    run.freeze()
    proxy = np.stack(
        [run.data.arrays["target_mean"], np.ones((run.data.rows, 2), np.float32)],
        axis=-1,
    )
    run.array_save(assembled_path(1), proxy)
    for inner in (1, 2, 3):
        run.array_save(assembled_path(1, inner), proxy)
        for shard in run.data.shards:
            offsets = correct.selected(run.data, shard, 1, inner, False)
            truth = run.data.native(shard, "y")[offsets]
            for target in (0, 1):
                run.save(
                    f'predictions/{correct.name("mean", 1, inner, target)}/{shard["name"]}.npz',
                    offset=offsets,
                    mu=truth[:, target] + 5,
                    scale=np.ones(len(offsets), np.float32),
                )
    return run, proxy


def test_nested_labels_and_prediction_boundary(training_data, tmp_path, monkeypatch):
    run, proxy = calibration_run(training_data, tmp_path)
    correct.assemble(run, 1)
    shard = run.data.shards[0]
    nested = run.load("cache/nested_o1/sample.npz")
    offsets = correct.selected(run.data, shard, 1, 0, True)
    np.testing.assert_array_equal(nested["offset"], offsets)
    np.testing.assert_allclose(nested["logk"], np.log(5 / (Q90 + EPS)), rtol=1e-6)
    assert not (
        run.data.folds.fold.to_numpy()[
            run.data.native(shard, "row")[offsets] % run.data.n
        ]
        == 1
    ).any()
    native = run.data.native

    def guarded(shard, key):
        if key in ("y", "route"):
            raise AssertionError("Prediction read held-out labels")
        return native(shard, key)

    monkeypatch.setattr(run.data, "native", guarded)
    offsets, matrix, location, scale = correct.inference_data(
        run, "mean", shard, 1, 0, 0, proxy
    )
    assert matrix.shape == (len(offsets), 61)
    run.save(
        "predictions/mean_o1_j0_NEE/sample.npz",
        offset=offsets,
        mu=location,
        scale=scale,
    )
    _, matrix, _, _ = correct.inference_data(run, "quantile", shard, 1, 0, 0, proxy)
    assert matrix.shape == (len(offsets), 67)


def test_cpu_external_memory_both_objectives(training_data, tmp_path):
    run, _ = calibration_run(training_data, tmp_path)
    correct.assemble(run, 1)
    for mode in ("mean", "quantile"):
        key = correct.name(mode, 1, 0, 0)
        outputs = correct.outputs(run.data, mode, 1, 0, 0)
        run.stage(key, outputs, lambda: correct.fit(run, mode, 1, 0, 0))
        model = xgb.Booster()
        model.load_model(run.root / outputs[0])
        assert model.num_boosted_rounds() == 200
        assert read(run.root / outputs[1])["checkpoint_reload"]
        assert not run.stage(
            key, outputs, lambda: pytest.fail("Repeated corrector training")
        )
    values = run.load("predictions/quantile_o1_j0_NEE/sample.npz")
    assert (values["k"] >= 1).all()


def test_empty_interval_holdout(training_data, tmp_path, monkeypatch):
    run, proxy = calibration_run(training_data, tmp_path)
    monkeypatch.setattr(correct, "selected", lambda *args: np.array([], dtype=np.int64))
    monkeypatch.setattr(
        run, "load", lambda *args: pytest.fail("Empty fold read prediction file")
    )
    offsets, matrix, _, _ = correct.inference_data(
        run, "quantile", run.data.shards[0], 1, 0, 0, proxy
    )
    assert matrix.shape == (0, 67)
    assert len(offsets) == 0


@pytest.mark.parametrize("mode", ["mean", "quantile"])
def test_interrupted_fit_matches_uninterrupted(training_data, tmp_path, monkeypatch, mode):
    run, proxy = calibration_run(training_data, tmp_path)
    # Vary the labels so row/column sampling affects the learned trees.
    proxy[:, :, 0] += run.data.arrays["numeric"][:, :2] * 5
    run.array_save(assembled_path(1), proxy)
    correct.assemble(run, 1)
    shard = run.data.shards[0]
    nested = run.load("cache/nested_o1/sample.npz")
    nested["logk"] += np.abs(run.data.native(shard, "x")[nested["offset"], :2])
    run.save("cache/nested_o1/sample.npz", **nested)
    if mode == "quantile":
        correct.fit(run, "mean", 1, 0, 0)
    key = correct.name(mode, 1, 0, 0)
    correct.fit(run, mode, 1, 0, 0)
    path = run.root / f"models/{key}.json"
    expected = xgb.Booster(model_file=path).get_dump()
    predictions = run.load(f"predictions/{key}/sample.npz")
    path.rename(run.root / "uninterrupted-model.json")
    callback = correct.Progress.after_iteration

    class Interrupted(Exception):
        pass

    def interrupt(self, model, epoch, evals_log):
        result = callback(self, model, epoch, evals_log)
        if epoch == 9:
            raise Interrupted
        return result

    with monkeypatch.context() as patch:
        patch.setattr(correct.Progress, "after_iteration", interrupt)
        with pytest.raises(Interrupted):
            correct.fit(run, mode, 1, 0, 0)
    assert not path.exists()
    assert read(run.root / f"validation/{key}_progress.json")["rounds"] == 10
    run.stage(key, correct.outputs(run.data, mode, 1, 0, 0),
              lambda: correct.fit(run, mode, 1, 0, 0))
    assert xgb.Booster(model_file=path).get_dump() == expected
    for name, values in predictions.items():
        np.testing.assert_array_equal(
            run.load(f"predictions/{key}/sample.npz")[name], values
        )

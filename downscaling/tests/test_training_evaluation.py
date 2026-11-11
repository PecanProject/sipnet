import numpy as np
import pandas as pd
import pytest
import h5py
from downscaling.artifacts import write, sha, digest
from downscaling.train.state import Run
from downscaling.train.configuration import Config
from downscaling.train import evaluate, correct
from downscaling.train.history import Historical, ARRAYS
from downscaling.tests.test_training_data import training_data


def test_spatial_excludes_test_and_self():
    cells = np.full((3, 96, 2), np.nan)
    cells[:] = np.array([1, 3, 10000])[:, None, None]
    train, query = np.array([True, True, False]), np.array([False, False, True])
    coords = np.array([[0, 0], [0, 1], [0, 0.5]])
    prediction, _ = evaluate.smooth(cells, train, query, coords, np.ones(3))
    np.testing.assert_allclose(prediction[2], 2)
    cells[2] = -99999
    again, _ = evaluate.smooth(cells, train, query, coords, np.ones(3))
    np.testing.assert_array_equal(prediction, again)
    loo, _ = evaluate.smooth(cells, train, train, coords, np.ones(3), True)
    np.testing.assert_allclose(loo[0], 3)
    np.testing.assert_allclose(loo[1], 1)
    assert evaluate.weighted_quantile([1, 10, 100], np.array([999, 1, 0])) == 1


def test_scores_export_and_unchanged_interval_scale(training_data, tmp_path):
    run = Run(Config(training_data, tmp_path / "run"))
    run.freeze()
    for outer in (1, 2, 3):
        for shard in run.data.shards:
            offsets = correct.selected(run.data, shard, outer, 0, True)
            truth = run.data.native(shard, "y")[offsets]
            run.save(
                f'cache/nested_o{outer}/{shard["name"]}.npz',
                offset=offsets,
                mu=truth + 5,
            )
            held = correct.selected(run.data, shard, outer, 0, False)
            truth = run.data.native(shard, "y")[held]
            for target, label in enumerate(("NEE", "LE")):
                run.save(
                    f'predictions/mean_o{outer}_j0_{label}/{shard["name"]}.npz',
                    offset=held,
                    mu=truth[:, target] + 5,
                    scale=np.ones(len(held), np.float32),
                )
                run.save(
                    f'predictions/quantile_o{outer}_j0_{label}/{shard["name"]}.npz',
                    offset=held,
                    k=np.full(len(held), 2, np.float32),
                )
        evaluate.spatial(run, outer)
    evaluate.report(run)
    values = run.load("predictions/final/sample.npz")
    np.testing.assert_allclose(
        values["location"], run.data.native(run.data.shards[0], "y"), rtol=0, atol=1e-6
    )
    np.testing.assert_array_equal(values["scale"], 2)
    for label in ("period", "month"):
        with h5py.File(run.root / f"predictions/{label}_oof.h5") as handle:
            assert handle["support"][:].sum() == 108
            np.testing.assert_array_equal(
                handle["site_ids"][:], run.data.arrays["site_ids"]
            )
    metrics = pd.read_csv(run.root / "tables/metrics.csv")
    assert set(metrics.target) == {"NEE", "LE"}
    assert set(metrics.scale) == {"3h", "period", "month"}
    values = evaluate.contributions([0.0, 2.0], [1.0, 1.0], [1.0, 1.0])
    scores = evaluate.summary(values.sum(axis=0))
    assert scores["rmse"] == 1 and scores["bias"] == 0


def history_fixture(root):
    root.mkdir()
    pd.DataFrame(dict(index=[9, 2], fold=[3, 1])).to_csv(
        root / "sites.csv", index=False
    )
    pd.DataFrame(dict(utc=["2020-01-01T00:00:00Z"])).to_csv(
        root / "steps.csv", index=False
    )
    pd.DataFrame(
        [dict(fold=f, target=t, cap=float(f)) for f in (3, 1, 2) for t in ("LE", "NEE")]
    ).to_csv(root / "thresholds.csv", index=False)
    for key in ARRAYS:
        values = np.ones((2, 1, 2))
        if key == "nearby_calendar_LC":
            values = np.full((2, 96, 2), 100.0)
        if key == "truth":
            values = np.array([[[90.0, 91.0]], [[20.0, 21.0]]])
        if key == "eligible":
            values = np.ones((2, 1), bool)
        np.save(root / f"{key}.npy", values)
    files = {
        name: sha(root / name)
        for name in [
            "sites.csv",
            "steps.csv",
            "thresholds.csv",
            *[f"{key}.npy" for key in ARRAYS],
        ]
    }
    payload = dict(schema="sipnet-downscaling-history-1", files=files)
    write(root / "manifest.json", dict(id=digest(payload), **payload))
    return Historical(root)


def test_history_uses_its_own_ids_and_folds(tmp_path):
    history = history_fixture(tmp_path / "history")
    y, mu, scale, valid = history.values(
        [2, 9, 2],
        pd.to_datetime(["2020-01-01T00:00Z", "2020-01-01T00:00Z", "2019-01-01T00:00Z"]),
    )
    np.testing.assert_array_equal(y[:2], [[20, 21], [90, 91]])
    np.testing.assert_array_equal(mu[:2], [[20, 21], [88, 89]])
    np.testing.assert_array_equal(valid, [True, True, False])
    with pytest.raises(ValueError, match="site IDs"):
        history.values([123], pd.to_datetime(["2020-01-01T00:00Z"]))

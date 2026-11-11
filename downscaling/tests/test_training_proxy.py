import numpy as np
import pytest
import torch
from downscaling.artifacts import read
from downscaling.train.configuration import Config
from downscaling.train.optimization import prepare
from downscaling.train.proxy import (
    train,
    name,
    outputs,
    load_model,
    training_rows,
    infer,
)
from downscaling.train.state import Run
from downscaling.tests.test_training_data import training_data


def test_preprocessing_excludes_held_rows(training_data, tmp_path):
    run = Run(Config(training_data, tmp_path / "run"))
    arrays = {key: np.array(value) for key, value in run.data.arrays.items()}
    _, fit, validation, test, *_ = training_rows(run.data, 1)
    original = prepare(arrays, fit)
    held = np.concatenate([validation, test])
    arrays["numeric"][held] = 99999
    arrays["target_mean"][held] = -99999
    changed = prepare(arrays, fit)
    assert original.preprocessing == changed.preprocessing
    np.testing.assert_array_equal(original.numeric[fit], changed.numeric[fit])
    np.testing.assert_array_equal(original.target_center, changed.target_center)
    np.testing.assert_array_equal(original.target_scale, changed.target_scale)
    assert original.target_members.shape[-1] == 1


def test_full_recipe_proxy_reload_and_resume(training_data, tmp_path):
    run = Run(Config(training_data, tmp_path / "run"))
    run.freeze()
    key = name(1)
    expected = outputs(1)
    assert run.stage(key, expected, lambda: train(run, 1))
    model, checkpoint = load_model(run, key)
    assert model.backbone.lc_embedding.num_embeddings == 12
    assert model.backbone.year_embedding is None
    rows, _, _, test, *_ = training_rows(run.data, 1)
    prepared = prepare(run.data.arrays, rows)
    result = infer(model, prepared, test)
    saved = run.load(f"predictions/{key}.npz")
    np.testing.assert_allclose(result[:, :, 0], saved["location"], rtol=0, atol=1e-6)
    before = (run.root / expected[0]).stat().st_mtime_ns
    assert not run.stage(
        key, expected, lambda: pytest.fail("Completed stage ran again")
    )
    assert (run.root / expected[0]).stat().st_mtime_ns == before
    assert checkpoint["metadata"]["settings"]["max_epochs"] == 80
    audit = read(run.root / expected[2])
    assert len(audit["refit_history"]) == audit["best_epoch"]
    (run.root / expected[1]).rename(run.root / "removed-prediction.npz")
    with pytest.raises(ValueError, match="Missing artifact"):
        run.stage(
            key, expected, lambda: pytest.fail("Damaged completion triggered training")
        )

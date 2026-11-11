"""Fast bundle integrity checks and optional acquired-model CPU/CUDA parity."""

import os
from pathlib import Path
import numpy as np
import pandas as pd
import pytest
from downscaling.artifacts import write, sha, digest
from downscaling.models.bundle import SCHEMA, feature_contract, required_files, validate


def test_bundle_integrity(tmp_path):
    files = {}
    for name in required_files():
        path = tmp_path / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(b"model")
        files[name] = sha(path)
    manifest = dict(schema=SCHEMA, contract=feature_contract(), files=files)
    manifest["id"] = digest(manifest)
    write(tmp_path / "manifest.json", manifest)
    assert validate(tmp_path) == manifest
    path = tmp_path / next(iter(files))
    path.write_bytes(b"changed")
    with pytest.raises(ValueError, match="hash mismatch"):
        validate(tmp_path)
    path.unlink()
    with pytest.raises(ValueError, match="Missing model"):
        validate(tmp_path)
    manifest["contract"]["proxy_lc"] = [1]
    write(tmp_path / "manifest.json", manifest)
    with pytest.raises(ValueError, match="feature contract"):
        validate(tmp_path)


def test_acquired_model_replay():
    root = os.environ.get("DOWNSCALING_VALIDATION_ROOT")
    if not root:
        pytest.skip("Set DOWNSCALING_VALIDATION_ROOT for acquired-model replay")
    from downscaling.models.inference import Models, native_matrix
    from downscaling.contracts import predictors

    root = Path(root)
    reference = root / "reference"
    manifest = __import__("json").loads((reference / "manifest.json").read_text())
    assert all(
        sha(reference / name) == expected
        for name, expected in manifest["files"].items()
    )
    models = Models(
        root / "model_bundle", device=os.environ.get("DOWNSCALING_TEST_DEVICE", "cpu")
    )
    proxy = pd.read_parquet(reference / "proxy.parquet")
    with np.load(reference / "proxy.npz") as expected:
        numeric, lc, valid = predictors(proxy)
        np.testing.assert_array_equal(numeric, expected["numeric"])
        np.testing.assert_array_equal(lc, expected["lc"])
        prediction, mask = models.proxy(proxy)
        np.testing.assert_array_equal(mask, expected["valid"])
        np.testing.assert_allclose(
            prediction, expected["prediction"], rtol=1e-5, atol=2e-5
        )
    for year in (2012, 2018, 2024):
        frame = pd.read_parquet(reference / f"native_{year}.parquet")
        matrix = native_matrix(frame)
        with np.load(reference / f"native_{year}.npz") as expected:
            np.testing.assert_array_equal(matrix, expected["matrix"])
            # Zero proxy and spatial bias isolate the actual native correction interface.
            values = models.native(
                frame, np.zeros((3, len(frame), 2)), np.zeros((3, len(frame), 96, 2))
            )
            np.testing.assert_allclose(
                values, -expected["residual"], rtol=1e-6, atol=1e-5
            )
    for fold in (1, 2, 3):
        points = pd.read_parquet(reference / f"spatial_{fold}.parquet")
        bias, distance = models.spatial[fold].query(points, points.LC.to_numpy())
        with np.load(reference / f"spatial_{fold}.npz") as expected:
            np.testing.assert_allclose(bias, expected["bias"], rtol=1e-12, atol=1e-12)
            np.testing.assert_allclose(
                distance, expected["distance"], rtol=1e-12, atol=1e-12
            )

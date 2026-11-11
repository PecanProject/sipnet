import numpy as np
import pytest
import rasterio
from downscaling.artifacts import receipt
from downscaling.predict.products import export, report, inventory, BANDS
from test_resume import publish_blocks


def test_empty_products_do_not_complete(run_case):
    config, contract, _, wanted = run_case
    publish_blocks(run_case)
    with pytest.raises(ValueError, match="Missing artifact"):
        report(config, contract, wanted)
    assert not config.destination("COMPLETE.json").exists()


@pytest.mark.parametrize("all_excluded", [False, True])
def test_export_reopen_and_resume(run_case, all_excluded):
    config, contract, _, wanted = run_case
    publish_blocks(run_case, excluded_all=all_excluded)
    export(config, contract, wanted)
    result = report(config, contract, wanted)
    assert result["status"] == "complete" and len(result["products"]) == 4
    before = {p: config.destination(p).stat().st_mtime_ns for p in result["products"]}
    export(config, contract, wanted)
    assert report(config, contract, wanted) == result
    assert before == {
        p: config.destination(p).stat().st_mtime_ns for p in result["products"]
    }
    for item in inventory(config, wanted):
        with rasterio.open(item["path"]) as ds:
            assert ds.shape == (2, 2) and ds.descriptions == BANDS
            assert ds.transform == rasterio.Affine(0.1, 0, -89.9, 0, -0.1, 44.9)
            assert ds.tags()["expected_steps"] == "8"
            if all_excluded:
                assert np.isnan(ds.read([1, 2, 3, 4, 5])).all()
                assert (ds.read(12) == 0).all()
                assert (ds.read(6) == 0).all() and (ds.read(10) == 4).all()
            else:
                np.testing.assert_array_equal(ds.read(6).ravel(), [8, 4, 0, 0])
                np.testing.assert_array_equal(ds.read(10).ravel(), [0, 0, 2, 4])


@pytest.mark.parametrize(
    "damage", ["missing_LE", "old_token", "bad_grid", "bad_support", "missing_QA"]
)
def test_completion_checks_required_products(run_case, damage):
    config, contract, _, wanted = run_case
    base = publish_blocks(run_case)
    export(config, contract, wanted)
    report(config, contract, wanted)
    path = next(
        item["path"] for item in inventory(config, wanted) if item["target"] == "LE"
    )
    if damage == "missing_LE":
        path.rename(path.with_suffix(".removed"))
    elif damage == "missing_QA":
        (base / "period_001_qa.json").rename(base / "qa.removed")
    elif damage == "old_token":
        receipt(path, "old")
    else:
        with rasterio.open(path, "r+") as ds:
            if damage == "bad_grid":
                ds.transform = rasterio.Affine.identity()
            else:
                ds.write(np.full((2, 2), 9, np.float32), 6)
        receipt(path, contract["token"])
    with pytest.raises(ValueError):
        report(config, contract, wanted)


def test_disjoint_month_coverage_is_explicit(run_case):
    import pandas as pd

    config, _, _, wanted = run_case
    second = wanted.copy()
    second["period_id"] = 2
    second["start"] += pd.Timedelta(days=2)
    second["stop"] += pd.Timedelta(days=2)
    products = inventory(config, pd.concat([wanted, second]))
    month = next(item for item in products if item["scale"] == "month")
    assert month["expected"] == 16 and len(month["segments"]) == 2

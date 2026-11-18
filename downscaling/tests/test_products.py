import pytest
import rasterio
from downscaling.predict.products import products, path_for
from downscaling.artifacts import receipt
from test_resume import publish_blocks


@pytest.mark.parametrize("excluded", [False, True])
def test_period_inventory_and_timezone_support(run_case, excluded):
    cfg, manifest, _, wanted = run_case
    row = next(wanted.itertuples())
    publish_blocks(run_case, excluded)
    products(cfg, manifest, row, True)
    products(cfg, manifest, row, False)
    path = path_for(cfg, row, "LE")
    with rasterio.open(path) as ds:
        assert (
            ds.count == 7
            and (ds.read(6) == (0 if excluded else 1)).all()
            and (ds.read(7) == (4 if excluded else 0)).all()
        )
    path.rename(cfg.output / "missing.tif")
    with pytest.raises(ValueError, match="Missing"):
        products(cfg, manifest, row, False)


@pytest.mark.parametrize("fault", ["block", "qa", "token"])
def test_incomplete_blocks(run_case, fault):
    cfg, manifest, _, wanted = run_case
    row = next(wanted.itertuples())
    p = publish_blocks(run_case)
    if fault == "token":
        receipt(p, "old")
    else:
        if fault == "qa":
            p = p.with_suffix(".json")
        p.rename(cfg.output / "removed")
    with pytest.raises(ValueError):
        products(cfg, manifest, row, True)

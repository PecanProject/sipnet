import numpy as np
import pandas as pd
import pytest
from downscaling.artifacts import write, receipt, save_npz
from downscaling.predict.state import plan, locked
from downscaling.predict.engine import (
    block_base,
    frame_save,
    block_inventory,
    verify_blocks,
)
from downscaling.prepare.domain import points_for_tile
from downscaling.prepare.sources import Sources


def publish_blocks(case, excluded_all=False):
    config, contract, needed, wanted = case
    token = contract["token"]
    tile = contract["tiles"][0]
    points, _ = points_for_tile(config, Sources(config.sources), tile)
    points["tz"] = "UTC"
    excluded = points.copy() if excluded_all else points.iloc[-1:].copy()
    excluded["tz"] = None
    points = points.iloc[:0] if excluded_all else points.iloc[:-1]
    base = block_base(config, tile)
    frame_save(base / "points.parquet", points, token)
    frame_save(base / "excluded.parquet", excluded, token)
    if len(points):
        count = np.array([8, 4, 0], np.uint32)
        fold = np.ones((3, 3, 2), np.float32)
        fold[:, 2, :] = np.nan
        arrays = dict(
            pixel=points["index"].to_numpy(),
            fold_mean=fold,
            count=count,
            expected=np.int32(8),
            distance_km=np.ones((3, 3)),
            reason=np.array([0, 0, 2], np.uint8),
            vi_filled_steps=np.zeros(3, np.uint16),
            longest_missing_steps=8 - count,
            missing_steps=8 - count,
        )
        arrays.update(
            {
                "sum_2018-01": np.nan_to_num(fold) * count[None, :, None],
                "count_2018-01": count,
                "filled_2018-01": np.zeros(3, np.uint16),
                "longest_2018-01": 8 - count,
                "prefix_2018-01": np.zeros(3, np.uint16),
                "suffix_2018-01": 8 - count,
                "expected_2018-01": np.int32(8),
            }
        )
        save_npz(base / "period_001.npz", token, **arrays)
        write(base / "period_001_qa.json", dict(period_id=1, points=3))
        receipt(base / "period_001_qa.json", token)
    inventory, _ = block_inventory(config, contract, wanted)
    write(config.destination("manifest.json"), contract)
    write(config.destination("blocks_complete.json"), inventory)
    receipt(config.destination("blocks_complete.json"), token)
    return base


def test_preflight_is_read_only_and_frozen(run_case):
    config, contract, _, _ = run_case
    assert not config.output.exists()
    write(config.destination("manifest.json"), contract)
    path = config.destination("manifest.json")
    before = (path.read_bytes(), path.stat().st_mtime_ns)
    assert plan(config)[0] == contract
    assert before == (path.read_bytes(), path.stat().st_mtime_ns)
    config.sources["kgc"].write_bytes(b"changed")
    with pytest.raises(ValueError, match="source changed"):
        plan(config)
    assert before == (path.read_bytes(), path.stat().st_mtime_ns)


def test_exclusive_run_lock(run_case):
    config = run_case[0]
    with locked(config):
        with pytest.raises(RuntimeError, match="Another process"):
            with locked(config):
                pass


@pytest.mark.parametrize(
    "name",
    [
        "period_001.npz",
        "period_001_qa.json",
        "period_001.npz.ok.json",
        "points.parquet",
        "excluded.parquet",
    ],
)
def test_missing_block_or_receipt_fails(run_case, name):
    config, contract, _, wanted = run_case
    base = publish_blocks(run_case)
    assert verify_blocks(config, contract, wanted)
    (base / name).rename(base / (name + ".removed"))
    with pytest.raises(ValueError, match="Missing artifact"):
        verify_blocks(config, contract, wanted)


def test_cached_blocks_do_not_hide_source_disappearance(run_case):
    config, contract, _, wanted = run_case
    publish_blocks(run_case)
    path = config.sources["aqua_catalog"]
    path.rename(path.with_suffix(".removed"))
    with pytest.raises(ValueError, match="source disappeared"):
        verify_blocks(config, contract, wanted)


def test_old_token_is_rejected(run_case):
    config, contract, _, wanted = run_case
    base = publish_blocks(run_case)
    receipt(base / "period_001.npz", "old")
    with pytest.raises(ValueError, match="Stale"):
        verify_blocks(config, contract, wanted)


def test_request_change_is_rejected(run_case):
    from dataclasses import replace

    config, contract, _, _ = run_case
    write(config.destination("manifest.json"), contract)
    with pytest.raises(ValueError, match="new output"):
        plan(replace(config, window=dict(row=1, col=1, height=1, width=2)))


@pytest.mark.parametrize("removed", ["receipt", "layer"])
def test_cached_blocks_verify_bound_aqua_representations(run_case, removed):
    from downscaling.artifacts import identity
    from downscaling.prepare.sources import aqua_tiles

    config, _, _, _ = run_case
    points, _ = points_for_tile(
        config, Sources(config.sources), dict(row=1, col=1, height=2, width=2)
    )
    tile = aqua_tiles(points)[0]
    title = f"MYD13Q1.A2018009.{tile}.061.TEST"
    raw = config.sources["aqua_receipts"] / "source.hdf"
    layer = config.sources["aqua_receipts"] / "source_EVI.tif"
    raw.write_bytes(b"raw")
    layer.write_bytes(b"layer")
    receipt_path = config.sources["aqua_receipts"] / f"{title}.json"
    write(receipt_path, dict(raw=identity(raw), layers=[identity(layer)]))
    write(
        config.sources["aqua_catalog"],
        dict(start="2017-01-01", stop="2019-01-01", granules=[dict(title=title)]),
    )
    contract, needed, wanted = plan(config)
    publish_blocks((config, contract, needed, wanted))
    path = receipt_path if removed == "receipt" else layer
    path.rename(path.with_suffix(".removed"))
    with pytest.raises(ValueError, match="source disappeared"):
        verify_blocks(config, contract, wanted)


def test_native_block_exclusions_use_actual_eligibility(tmp_path):
    from types import SimpleNamespace
    from downscaling.contracts import STATIC, DYNAMIC, predictors, KGC
    from downscaling.predict.engine import period_block

    fields = (
        STATIC + DYNAMIC + [f"{name}_L{lag}" for lag in range(1, 5) for name in DYNAMIC]
    )
    panel = pd.DataFrame({name: [0.2] * 3 for name in fields})
    panel["lat"] = 45.0
    panel["lon"] = -90.0
    panel["LC_proxy"] = 1
    panel["period_key"] = "2020-02-29"
    panel.loc[2, "temp_c"] = np.nan
    points = pd.DataFrame(dict(index=[1, 2, 3], lat=[45.0] * 3, lon=[-90.0] * 3))

    class Spatial:
        def query(self, points, lc):
            return np.zeros((len(points), 96, 2)), np.ones(len(points))

    class Models:
        spatial = {fold: Spatial() for fold in (1, 2, 3)}

        def proxy(self, panel):
            return np.ones((3, len(panel), 2), np.float32), predictors(panel)[2]

        def native(self, frame, proxy, bias):
            assert frame["index"].tolist() == [1]
            return np.ones((3, len(frame), 2), np.float32)

    class Stream:
        static = {2020: dict(LC=np.array([1, 11, 1]), KGC=np.array([KGC[0]] * 3))}

        def at(self, time):
            frame = points.copy()
            frame["LC"] = [1, 11, 1]
            frame["KGC"] = KGC[0]
            for name in (
                "t2m",
                "sp",
                "d2m",
                "tp",
                "strd",
                "PPFD",
                "NDVI",
                "EVI",
                "sm_0_10",
                "sm_10_40",
                "sm_40_100",
                "sm_100_200",
            ):
                frame[name] = np.nan
            frame["EVI_filled"] = 0
            frame["NDVI_filled"] = 0
            return frame

    row = SimpleNamespace(
        period_id=1,
        start=pd.Timestamp("2020-02-29", tz="UTC"),
        stop=pd.Timestamp("2020-02-29 06:00", tz="UTC"),
    )
    period_block(Models(), points, row, panel, Stream(), tmp_path, "test")
    with np.load(tmp_path / "period_001.npz") as actual:
        np.testing.assert_array_equal(actual["count"], [2, 0, 0])
        np.testing.assert_array_equal(actual["reason"], [0, 2, 1])
        np.testing.assert_array_equal(actual["longest_missing_steps"], [0, 2, 2])
        assert np.isnan(actual["fold_mean"][:, 1:]).all()

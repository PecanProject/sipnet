import numpy as np
import pandas as pd
import pytest
from downscaling.artifacts import write, receipt, save_npz, read
from downscaling.predict.state import plan, locked, verify_ledger
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

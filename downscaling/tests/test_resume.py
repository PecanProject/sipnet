import numpy as np
import pytest
from downscaling.artifacts import write
from downscaling.predict.products import block, store
from downscaling.predict.state import plan, locked


def publish_blocks(case, excluded=False):
    cfg, manifest, _, wanted = case
    row = next(wanted.itertuples())
    tile = manifest["tiles"][0]
    ids = np.array([5, 6, 9, 10])
    from pandas import DataFrame

    store(
        cfg,
        row,
        tile,
        dict(
            land=ids,
            meta=dict(period_panels=[]),
            reason_ids=ids[:0] if excluded else ids,
            reason=np.array([]) if excluded else np.zeros(len(ids)),
            points=DataFrame(dict(index=ids)),
        ),
        None if excluded else np.ones((3, len(ids), 2), np.float32),
        manifest["token"],
    )
    return block(cfg, row, tile)


def test_lock_and_source_identity(run_case):
    cfg, manifest, *_ = run_case
    assert not cfg.output.exists()
    with locked(cfg):
        with pytest.raises(RuntimeError), locked(cfg):
            pass
    write(cfg.destination("manifest.json"), manifest)
    assert plan(cfg)[0] == manifest
    cfg.sources["aqua_ledger"].rename(cfg.output / "moved.json")
    with pytest.raises(ValueError, match="disappeared"):
        plan(cfg)

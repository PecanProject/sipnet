"""Accumulated fluxes must be compared in the native flux units."""

import numpy as np
import pytest
from downscaling.scripts.validate import compare_block_arrays


def block(delta=0.0):
    count = np.array([128, 4, 0], np.uint32)
    sums = np.zeros((3, 3, 2), np.float64)
    sums[:, :2, :] = delta * count[None, :2, None]
    return {"count_2020-01": count, "sum_2020-01": sums, "pixel": np.arange(3)}


def test_accumulated_roundoff_uses_unchanged_native_tolerance():
    # Each supported value differs by 2e-6, below the native 1e-5 atol.
    # Its 128-step sum differs by 2.56e-4 and must not use the single-value atol.
    compare_block_arrays(block(2e-6), block())


def test_out_of_tolerance_native_difference_still_fails():
    with pytest.raises(AssertionError):
        compare_block_arrays(block(2e-5), block())


def test_partial_support_uses_actual_count():
    actual = block()
    actual["sum_2020-01"][:, 1, :] = 8e-5
    with pytest.raises(AssertionError):
        compare_block_arrays(actual, block())


@pytest.mark.parametrize("damage", ["count", "zero_support", "shape", "nonfinite"])
def test_integrity_checks_are_not_normalized_away(damage):
    actual = block()
    if damage == "count":
        actual["count_2020-01"][1] = 5
    elif damage == "zero_support":
        actual["sum_2020-01"][:, 2, :] = 1e-8
    elif damage == "shape":
        actual["sum_2020-01"] = actual["sum_2020-01"][:1]
    else:
        actual["sum_2020-01"][:, 0, :] = np.nan
    with pytest.raises(AssertionError):
        compare_block_arrays(actual, block())


def test_module_audit_tolerates_lazy_import(monkeypatch, tmp_path):
    from types import SimpleNamespace
    from downscaling.scripts import validate

    source = tmp_path / "module.py"
    source.write_text("# fixture\n")
    registry = {}

    class LazyModule:
        @property
        def __file__(self):
            registry["newly_imported"] = SimpleNamespace(__file__=str(source))
            return str(source)

    registry["lazy"] = LazyModule()
    monkeypatch.setattr(validate, "sys", SimpleNamespace(modules=registry))
    assert validate.loaded_module_paths() == [str(source.resolve())]

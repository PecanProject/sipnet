import pytest
from downscaling.train.state import Run
from downscaling.train.correct import prepare
from downscaling.train.configuration import Config


def test_partial_periods_excluded(training_data, tmp_path):
    run = Run(Config(training_data, tmp_path / "run"))
    with pytest.raises(ValueError, match="full three-hour support"):
        prepare(run)

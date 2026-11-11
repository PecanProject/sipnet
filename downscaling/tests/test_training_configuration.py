import importlib
import sys
import subprocess
import pytest
import yaml
from downscaling.train.configuration import load
from downscaling.train.recipe import contract


def test_training_configuration_is_read_only(tmp_path):
    path = tmp_path / "train.yaml"
    path.write_text(
        yaml.safe_dump(
            dict(
                prepared_data="data",
                output="run",
                resources=dict(device="cpu", cpu_threads=2),
            )
        )
    )
    config = load(path)
    assert config.output == tmp_path / "run"
    assert not config.output.exists()
    assert not config.prepared_data.exists()
    importlib.import_module("downscaling.train")
    subprocess.run(
        [
            sys.executable,
            "-c",
            "import sys, downscaling.train; assert 'downscaling.train.proxy' not in sys.modules",
        ],
        check=True,
    )


@pytest.mark.parametrize("extra", [dict(epochs=1), dict(seed=9)])
def test_no_scientific_override(tmp_path, extra):
    path = tmp_path / "train.yaml"
    path.write_text(
        yaml.safe_dump(
            dict(
                prepared_data="data",
                output="run",
                resources=dict(device="cpu", cpu_threads=1),
                **extra,
            )
        )
    )
    with pytest.raises(ValueError, match="Training config"):
        load(path)


def test_effective_recipe():
    recipe = contract()
    assert recipe["candidate"] == dict(
        width=256, blocks=4, sharing="shared", basis="none", use_year_embedding=False
    )
    assert recipe["settings"]["max_epochs"] == 80
    assert recipe["settings"]["patience"] == 10
    assert recipe["settings"]["batch_size"] == 8192
    assert recipe["rounds"] == 200
    assert recipe["seed"] == 42
    recipe["settings"]["max_epochs"] = 1
    assert contract()["settings"]["max_epochs"] == 80

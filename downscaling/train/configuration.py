"""Paths and execution resources; scientific settings belong to the recipe."""

from dataclasses import dataclass
from pathlib import Path
import yaml


@dataclass(frozen=True)
class Config:
    prepared_data: Path
    output: Path
    device: str = "cpu"
    cpu_threads: int = 1


def load(path):
    path = Path(path).resolve()
    values = yaml.safe_load(path.read_text())
    if not isinstance(values, dict) or set(values) != {"prepared_data", "output", "resources"}:
        raise ValueError("Training config requires prepared_data, output and resources")
    resources = values["resources"]
    if not isinstance(resources, dict) or set(resources) != {"device", "cpu_threads"}:
        raise ValueError("Training resources require device and cpu_threads")
    device, threads = resources["device"], resources["cpu_threads"]
    if device not in ("cpu", "cuda"):
        raise ValueError("Training device must be cpu or cuda")
    if type(threads) is not int or threads < 1:
        raise ValueError("cpu_threads must be a positive integer")
    paths = {}
    for name in ("prepared_data", "output"):
        value = values[name]
        if not isinstance(value, str) or not value.strip():
            raise ValueError(f"{name} must be a nonempty path")
        paths[name] = (path.parent / value).resolve()
    source, output = paths["prepared_data"], paths["output"]
    repository = Path(__file__).resolve().parents[2]
    if output.is_relative_to(repository):
        raise ValueError("Training outputs belong outside the code repository")
    if output.is_relative_to(source) or source.is_relative_to(output):
        raise ValueError("Prepared data and training output must be disjoint")
    return Config(source, output, device, threads)

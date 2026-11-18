import fcntl
import numpy as np
import pytest
from downscaling.artifacts import read, write, receipt
from downscaling.train.configuration import Config
from downscaling.train.state import Run
from downscaling.train.engine import execute, stages, status


def test_declared_training_schedule(training_data, tmp_path):
    run = Run(Config(training_data, tmp_path / "run"))
    schedule = stages(run.data)
    assert len({name for name, *_ in schedule}) == len(schedule)
    assert sum(task == "proxy" for _, _, task, _ in schedule) == 39
    assert sum(task == "correct" for _, _, task, _ in schedule) == 12
    assert sum(task == "spatial" for _, _, task, _ in schedule) == 3


def test_preflight_repeat_is_read_only_and_request_changes_fail(
    training_data, tmp_path
):
    config = Config(training_data, tmp_path / "run")
    first = execute("preflight", config)
    path = config.output / "manifest.json"
    before = path.read_bytes(), path.stat().st_mtime_ns
    assert execute("preflight", config) == first
    assert (path.read_bytes(), path.stat().st_mtime_ns) == before
    with pytest.raises(ValueError, match="changed"):
        execute("preflight", Config(training_data, config.output, "cpu", 2))
    assert execute("report", config)["status"] == "incomplete"


def test_stale_receipt_and_missing_product_never_retrain(training_data, tmp_path):
    run = Run(Config(training_data, tmp_path / "run"))
    run.freeze()
    expected = ["test/model.npy", "test/prediction.npy"]

    def produce():
        for name in expected:
            run.array_save(name, np.arange(4))

    run.stage("fixture", expected, produce)
    path = run.root / (expected[0] + ".ok.json")
    record = read(path)
    record["token"] = "old"
    write(path, record)
    with pytest.raises(ValueError, match="Stale"):
        run.stage("fixture", expected, lambda: pytest.fail("Stale stage retrained"))
    receipt(run.root / expected[0], run.token)
    (run.root / expected[1]).rename(run.root / "removed.npy")
    with pytest.raises(ValueError, match="Missing artifact"):
        run.stage("fixture", expected, lambda: pytest.fail("Missing output retrained"))


def test_empty_completion_and_concurrent_writer_rejected(training_data, tmp_path):
    config = Config(training_data, tmp_path / "run")
    execute("preflight", config)
    run = Run(config)
    run.json("COMPLETE.json", dict(status="complete", token=run.token, stages=[]))
    with pytest.raises(ValueError, match="Completion"):
        status(run, stages(run.data))
    with (config.output / "run.lock").open("a") as lock:
        fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        with pytest.raises(RuntimeError, match="Another process"):
            execute("run", config, resume=True)


def test_changed_prepared_input_rejected_before_resume(training_data, tmp_path):
    config = Config(training_data, tmp_path / "run")
    execute("preflight", config)
    path = training_data / "native/sample.y.npy"
    values = np.load(path)
    values[0] += 10
    np.save(path, values)
    with pytest.raises(ValueError, match="changed"):
        execute("run", config, resume=True)

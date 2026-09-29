"""Run the release-binary tutorial, validate results, and regenerate tutorial figures."""

import os
from pathlib import Path
import platform
import shutil
import subprocess
import sys
import tarfile
import tempfile
import zipfile

import numpy as np
import pandas as pd


RELEASE = "https://github.com/PecanProject/sipnet/releases/download/v2.2.0"


def download(url, path):
    subprocess.run(["curl", "--fail", "--location", "--silent", "--show-error",
                    "--retry", "2", "--max-time", "120", url, "-o", str(path)], check=True)


def validate(run):
    data = pd.read_csv(run / "sipnet.out", sep=r"\s+")
    climate = pd.read_csv(run / "sipnet.clim", sep=r"\s+", header=None)
    if len(data) != len(climate) or not np.isfinite(data.to_numpy()).all():
        raise ValueError("Expected finite output for every climate timestep")
    if not np.array_equal(data[["year", "day"]].to_numpy(), climate[[0, 1]].to_numpy()):
        raise ValueError("Output dates must match climate dates")
    if not np.allclose(data.time, climate[2], atol=0.005, rtol=0):
        raise ValueError("Output hours must match climate hours")
    selected = data[data.year == 2016]
    if set(selected.day) != set(range(1, 367)) or not np.isclose(climate.loc[climate[0] == 2016, 3].sum(), 366):
        raise ValueError("Expected all 366 days of 2016")
    # Printed fluxes and cumulative values are rounded to 0.001 g C/m2.
    if abs(selected.nee.sum() - selected.cumNEE.iloc[-1]) > (len(selected) + 1) * 0.0005:
        raise ValueError("Annual NEE does not match cumulative NEE")
    return data


def main():
    root = Path(__file__).resolve().parents[2]
    systems = {("Darwin", "arm64"): "macos-arm64", ("Linux", "x86_64"): "linux-x86_64"}
    asset = systems.get((platform.system(), platform.machine()))
    if asset is None:
        raise RuntimeError("Tutorial release binaries support macOS arm64 and Linux x86_64")
    with tempfile.TemporaryDirectory(prefix="sipnet-tutorial-") as temp:
        run = Path(temp)
        download(f"{RELEASE}/sipnet-{asset}-v2.2.0.tar.gz", run / "sipnet.tar.gz")
        with tarfile.open(run / "sipnet.tar.gz") as archive:
            (run / "sipnet").write_bytes(archive.extractfile("sipnet").read())
        (run / "sipnet").chmod(0o755)
        download(f"{RELEASE}/sipnet-tutorial-v2.2.0.zip", run / "tutorial.zip")
        with zipfile.ZipFile(run / "tutorial.zip") as archive:
            for filename in ["sipnet.clim", "sipnet.param", "events.in", "sipnet.in"]:
                (run / filename).write_bytes(archive.read(f"sipnet-tutorial/{filename}"))
        original = (run / "sipnet.param").read_text()
        old = "aMax 53.2895432752984\n"
        if original.count(old) != 1:
            raise ValueError("Expected one Russell Ranch aMax value")
        for scenario in ["base", "high"]:
            if scenario == "high":
                (run / "sipnet.param").write_text(original.replace(old, "aMax 63.94745193035808\n"))
            result = subprocess.run(["./sipnet", "-i", "sipnet.in"], cwd=run,
                                    capture_output=True, text=True, check=True)
            log = result.stdout + result.stderr
            if "[WARNING" in log or "[ERROR" in log:
                raise ValueError(f"Investigate {scenario} diagnostics:\n{log}")
            validate(run)
            if scenario == "base":
                for source, target in [("sipnet.param", "base.param"),
                                       ("sipnet.out", "base.out"),
                                       ("events.out", "base.events.out")]:
                    shutil.copyfile(run / source, run / target)
        env = os.environ.copy()
        env.update(MPLBACKEND="Agg")
        plot = root / "docs/user-guide/plot-tutorial.py"
        figures = root / "docs/user-guide/tutorial-1_files"
        for inputs in [["base.out"], ["base.out", "sipnet.out"]]:
            subprocess.run([sys.executable, str(plot), *inputs,
                            "--output-dir", str(figures)], cwd=run, env=env, check=True)
    print("Release tutorial scenarios and plots verified.")


if __name__ == "__main__":
    main()

"""Plot daily 2016 carbon fluxes from one or two SIPNET tutorial runs."""

import argparse
from pathlib import Path

import matplotlib.pyplot as plt
import pandas as pd


def daily_fluxes(filename):
    data = pd.read_csv(filename, sep=r"\s+")
    data.index = (pd.to_datetime(data.year.astype(str), format="%Y")
                  + pd.to_timedelta(data.day - 1, unit="D")
                  + pd.to_timedelta(data.time, unit="h"))
    selected = data.loc["2016", ["gpp", "nee"]]
    counts = selected.resample("D").size()
    expected = pd.date_range("2016-01-01", "2016-12-31", freq="D")
    if (not counts.index.equals(expected) or not counts.eq(8).all()
            or not selected.index.is_unique or not selected.index.is_monotonic_increasing):
        raise ValueError("Expected all 366 days of 2016 with eight timesteps per day")
    return selected.resample("D").sum()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("baseline", type=Path)
    parser.add_argument("modified", type=Path, nargs="?")
    parser.add_argument("--output-dir", type=Path, default=Path("."))
    args = parser.parse_args()
    args.output_dir.mkdir(parents=True, exist_ok=True)
    scenarios = [("Baseline", args.baseline, "-")]
    if args.modified:
        scenarios.append(("Higher aMax", args.modified, "--"))
    fig, axes = plt.subplots(2, 1, figsize=(8, 5), sharex=True)
    totals = {}
    for label, filename, style in scenarios:
        daily = daily_fluxes(filename)
        totals[label] = daily.sum()
        for axis, column in zip(axes, ["gpp", "nee"]):
            axis.plot(daily.index, daily[column], style, label=label)
            axis.set_ylabel(f"{column.upper()} (g C m⁻² day⁻¹)")
    axes[0].legend()
    axes[1].axhline(0, color="gray", linewidth=0.7)
    axes[1].set_xlabel("Date (2016)")
    fig.tight_layout()
    name = "comparison.png" if args.modified else "baseline.png"
    fig.savefig(args.output_dir / name, dpi=150)
    plt.close(fig)
    if args.modified:
        table = pd.DataFrame(totals)
        table["Difference"] = table["Higher aMax"] - table["Baseline"]
        table.to_csv(args.output_dir / "annual-totals.csv")
        print("Annual carbon totals (g C m⁻²); difference = higher aMax − baseline")
        print(table.round(2).to_string())


if __name__ == "__main__":
    main()

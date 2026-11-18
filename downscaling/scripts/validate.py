"""Replay a bounded request against published period maps."""

import argparse
import json
import numpy as np
import rasterio
from downscaling.configuration import load
from downscaling.predict.engine import execute
from downscaling.artifacts import write
from pathlib import Path


def validate(config, reference):
    result = execute("run", config, resume=True)
    w = config.window
    checked = []
    for name in result["products"]:
        product = config.output / name
        target, period = product.stem.split("_")
        path = reference / f"p{period}" / product.name
        with rasterio.open(path) as expected, rasterio.open(product) as actual:
            window = rasterio.windows.Window(
                w["col"], w["row"], w["width"], w["height"]
            )
            if (actual.transform, actual.crs, actual.descriptions) != (
                expected.window_transform(window),
                expected.crs,
                expected.descriptions,
            ):
                raise ValueError(f"Reference grid or bands disagree: {path}")
            for key in (
                "period_id",
                "target",
                "start_utc",
                "stop_utc_exclusive",
                "units",
            ):
                if actual.tags()[key] != expected.tags()[key]:
                    raise ValueError(f"Different {key}: {path}")
            for _, block in actual.block_windows(1):
                ref = rasterio.windows.Window(
                    window.col_off + block.col_off,
                    window.row_off + block.row_off,
                    block.width,
                    block.height,
                )
                np.testing.assert_array_equal(
                    actual.read(window=block), expected.read(window=ref)
                )
        checked.append(name)
    record = dict(status="passed", reference=str(reference), exact_products=checked)
    write(config.output / "validation.json", record)
    return record


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--config", required=True)
    parser.add_argument("--reference", type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(validate(load(args.config), args.reference)))


if __name__ == "__main__":
    main()

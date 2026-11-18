"""Check exported residual forests against held-out period predictions."""

import argparse, json
import numpy as np
import xgboost as xgb
from downscaling.train.configuration import load
from downscaling.train.engine import execute
from downscaling.train.proxy import assembled_path
from downscaling.train.state import Run
from downscaling.artifacts import write


def validate(config):
    if execute("report", config)["status"] != "complete":
        raise ValueError("Incomplete training run")
    run = Run(config)
    x = run.array("cache/period_features.npy")
    rows = run.load("cache/periods.npz")["rows"]
    for fold in (1, 2, 3):
        held = run.load(f"predictions/residual_o{fold}_j0.npz")
        test = held["test"][:128]
        base = run.array(assembled_path(fold))[rows[test], :, 0]
        for t, target in enumerate(("NEE", "LE")):
            model = xgb.Booster()
            model.load_model(
                run.root / f"model_bundle/residual/fold_{fold}_{target}.json"
            )
            model.set_param(dict(nthread=config.cpu_threads, device=config.device))
            values = x[test]
            if config.device == "cuda":
                import cupy as cp

                residual = model.inplace_predict(cp.asarray(values)).get()
            else:
                residual = model.inplace_predict(values)
            np.testing.assert_allclose(
                base[:, t] + residual,
                held["pred"][: len(test), t],
                rtol=2e-6,
                atol=2e-6,
            )
    record = dict(status="passed", folds=3, targets=["NEE", "LE"])
    write(run.root / "validation/reload.json", record)
    return record


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--config", required=True)
    print(json.dumps(validate(load(parser.parse_args().config))))

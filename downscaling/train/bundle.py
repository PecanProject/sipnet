"""Export deployment artifacts without training caches or historical scores."""

import shutil
import numpy as np
import torch
from downscaling.artifacts import write, sha, digest, receipt
from downscaling.models.bundle import SCHEMA, feature_contract, required_files, validate
from downscaling.train.proxy import load_model, name


def outputs():
    return [
        *[f"model_bundle/{path}" for path in sorted(required_files())],
        "model_bundle/manifest.json",
        "validation/bundle.json",
    ]


def export(run):
    root = run.path("model_bundle/manifest.json").parent
    for outer in (1, 2, 3):
        _, checkpoint = load_model(run, name(outer))
        destination = run.path(f"model_bundle/proxy/fold_{outer}.pt")
        temporary = destination.with_suffix(".partial")
        torch.save(
            {key: value.cpu() for key, value in checkpoint["state_dict"].items()},
            temporary,
        )
        temporary.replace(destination)
        write(run.path(f"model_bundle/proxy/fold_{outer}.json"), checkpoint["metadata"])
        for target in ("NEE", "LE"):
            shutil.copyfile(
                run.root / f"models/mean_o{outer}_j0_{target}.json",
                run.path(f"model_bundle/mean/fold_{outer}_{target}.json"),
            )
        spatial = run.load(f"models/spatial_o{outer}.npz")
        destination = run.path(f"model_bundle/spatial/fold_{outer}.npz")
        with destination.with_suffix(".partial").open("wb") as handle:
            np.savez_compressed(
                handle,
                **{
                    key: spatial[key]
                    for key in (
                        "cell",
                        "caps",
                        "train_sites",
                        "site_ids",
                        "lc",
                        "coords",
                    )
                },
            )
        destination.with_suffix(".partial").replace(destination)
    payload = dict(
        schema=SCHEMA,
        contract=feature_contract(),
        files={name: sha(root / name) for name in sorted(required_files())},
        provenance=dict(
            training_token=run.token,
            prepared_data_id=run.data.manifest["id"],
            code=run.manifest["code"],
            recipe=run.manifest["recipe"],
        ),
    )
    manifest = dict(id=digest(payload), **payload)
    write(root / "manifest.json", manifest)
    validate(root)
    for path in [*required_files(), "manifest.json"]:
        receipt(root / path, run.token)
    run.json(
        "validation/bundle.json",
        dict(
            status="passed",
            model_id=manifest["id"],
            files=len(required_files()),
            folds=3,
            targets=["NEE", "LE"],
        ),
    )

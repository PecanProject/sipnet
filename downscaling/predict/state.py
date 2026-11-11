"""Read-only request planning and immutable source/code binding."""

from pathlib import Path
from contextlib import contextmanager
import fcntl
import shutil
import pandas as pd
from downscaling.artifacts import identity, sha, digest, read
from downscaling.calendar import read_periods, selection
from downscaling.models.bundle import validate
from downscaling.prepare.sources import Sources
from downscaling.prepare.domain import tiles, points_for_tile


def code_identity():
    root = Path(__file__).parents[1]
    return {
        str(p.relative_to(root)): sha(p)
        for p in sorted(root.rglob("*"))
        if p.suffix in (".py", ".R")
        and not {"tests", "scripts"}.intersection(p.relative_to(root).parts)
    }


def verify_ledger(records):
    for record in records:
        path = record["path"]
        try:
            current = identity(path)
        except FileNotFoundError as error:
            raise ValueError(f"Bound source disappeared: {path}") from error
        if any(current[k] != record[k] for k in current) or (
            "sha256" in record and sha(path) != record["sha256"]
        ):
            raise ValueError(f"Bound source changed: {path}")


def plan(config):
    """Validate inputs before publishing anything, including on repeated preflight."""
    old = config.destination("manifest.json")
    if old.exists():
        verify_ledger(read(old)["sources"])
    bundle = validate(config.model_bundle)
    needed, wanted = selection(
        read_periods(config.sources["periods"]), config.period_ids
    )
    sources = Sources(config.sources)
    requested_tiles = tiles(config)
    records = {}

    def bind(path, hash_content=False):
        value = identity(path)
        if hash_content:
            value["sha256"] = sha(path)
        records[value["path"]] = value

    for key in (
        "grid",
        "periods",
        "gldas_registry",
        "native_lc",
        "kgc",
        "kgc_legend",
        "aqua_catalog",
        "timezones",
    ):
        if key in config.sources:
            bind(
                config.sources[key],
                key
                in (
                    "periods",
                    "gldas_registry",
                    "kgc_legend",
                    "aqua_catalog",
                    "timezones",
                ),
            )
    years = set(range(2013, 2025))
    native_years = set()
    era_years = set()
    for row in needed.itertuples():
        years.add(
            int(row.hydrological_year)
            if hasattr(row, "hydrological_year") and pd.notna(row.hydrological_year)
            else row.start.year + int(row.start.strftime("%m-%d") >= "07-16")
        )
        for time in pd.date_range(row.start, row.stop, freq="3h", inclusive="left"):
            records[str(sources.soil_path(time).resolve())] = sources.soil_identity(
                time
            )
            native_years.add(time.year)
            era_years.update((time.year, (time + pd.Timedelta(hours=3)).year))
    for year in sorted(years):
        bind(sources.annual(year))
    for year in sorted(native_years):
        bind(sources.native(year))
    for year in sorted(era_years):
        bind(config.sources["era"] / f"ERA5_{year}.nc")
    for tile in requested_tiles:
        points, _ = points_for_tile(config, sources, tile)
        tile["land_pixels"] = len(points)
        if not len(points):
            continue
        _, _, granules = sources.aqua_requirements(
            points, needed.start.min(), needed.stop.max()
        )
        for row in granules:
            receipt, record = sources.aqua_receipt(row)
            bind(receipt, True)
            for entry in [record["raw"], *record["layers"]]:
                bind(entry["path"])
    if not sum(t["land_pixels"] for t in requested_tiles):
        raise ValueError("Requested window contains no land pixels")
    contract = dict(
        schema="sipnet-downscaling-run-1",
        code=code_identity(),
        model_id=bundle["id"],
        window=config.window,
        period_ids=list(config.period_ids),
        tiles=requested_tiles,
        device=config.device,
        sources=[records[k] for k in sorted(records)],
        source_locations={k: str(v) for k, v in config.sources.items()},
    )
    contract["token"] = digest(contract)
    if old.exists() and read(old) != contract:
        raise ValueError(
            "Request, sources, model or code changed; use a new output directory"
        )
    return contract, needed, wanted


def budget(config, additional=0):
    existing = (
        sum(p.stat().st_size for p in config.output.rglob("*") if p.is_file())
        if config.output.exists()
        else 0
    )
    if existing + additional > config.max_output_bytes:
        raise ValueError("Configured output storage budget exceeded")
    parent = config.output
    while not parent.exists():
        parent = parent.parent
    if additional > shutil.disk_usage(parent).free:
        raise ValueError("Insufficient free output storage")


@contextmanager
def locked(config):
    config.output.mkdir(parents=True, exist_ok=True)
    with config.destination(".run.lock").open("a") as handle:
        try:
            fcntl.flock(handle, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError as error:
            raise RuntimeError("Another process owns this output directory") from error
        try:
            yield
        finally:
            fcntl.flock(handle, fcntl.LOCK_UN)

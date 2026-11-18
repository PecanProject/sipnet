import json, fcntl
import pyarrow as pa, pyarrow.parquet as pq
from downscaling.predict.inputs import point_signature, current_panel
from downscaling.contracts import DYNAMIC
from downscaling.artifacts import identity


def load_panels(points, needed, row, src, key, root, token):
    directory = root / key
    directory.mkdir(parents=True, exist_ok=True)
    with (directory / "write.lock").open("a") as lock:
        fcntl.flock(lock, fcntl.LOCK_EX)
        return read_or_build(points, needed, row, src, directory, token)


def read_or_build(points, needed, row, src, directory, token):
    coord = point_signature(points)
    panels, missing, receipts = {}, [], []
    for period in needed.itertuples():
        pid = int(period.period_id)
        path = directory / f"slot_{pid % 5}.parquet"
        sig = dict(
            period=pid,
            start=str(period.start),
            stop=str(period.stop),
            coords=coord,
            source=token[pid],
            soil=identity(src.paths["soil_cache"] / f"gldas_{pid:03d}.npy"),
        )
        hit = False
        if path.exists():
            meta = json.loads(pq.read_schema(path).metadata[b"history_panel"])
            if meta["period"] == pid:
                if meta != sig:
                    raise ValueError(f"Changed period panel inputs: {path}")
                columns = None if pid == row.period_id else ["index", *DYNAMIC]
                panels[pid] = pq.read_table(
                    path,
                    columns=columns,
                    use_threads=False,
                    page_checksum_verification=True,
                ).to_pandas(use_threads=False)
                hit = True
        if not hit:
            missing.append((period, path, sig))
        receipts.append(dict(period=pid, path=str(path), reused=hit))
    active = {row.period_id, *(r.period_id for r, _, _ in missing)}
    vi = src.vegetation(points, needed[needed.period_id.isin(active)])
    for period, path, sig in missing:
        panel = current_panel(points, vi, period, src, {})[0]
        table = pa.Table.from_pandas(panel, preserve_index=False)
        table = table.replace_schema_metadata(
            {**table.schema.metadata, b"history_panel": json.dumps(sig).encode()}
        )
        temp = path.with_suffix(".partial")
        pq.write_table(
            table,
            temp,
            compression="zstd",
            compression_level=1,
            write_page_checksum=True,
        )
        temp.replace(path)
        panels[period.period_id] = panel
    return panels, vi, receipts

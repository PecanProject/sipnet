#!/usr/bin/env python3
from __future__ import annotations

import argcomplete
import sys

from PySide6.QtWidgets import QApplication

from .sipnet_view_common import *

DEBUG_LOG_SOURCES = {
  "envi": "_envi.log",
  "flux": "_fluxes.log",
  "tracker": "_trackers.log",
}


def derive_prefix_from_log_file(log_path: Path) -> str:
  """Return the debug log prefix by stripping a known log suffix from *log_path*."""
  name = str(log_path)
  for suffix in DEBUG_LOG_SOURCES.values():
    if name.endswith(suffix):
      return name[: -len(suffix)]
  return name


class SipnetDebugViewerWindow(SipnetViewerWindowCore):
  def __init__(
      self,
      loaded: LoadedSipnetData,
      loaded_events: LoadedEventsData | None,
      initial_columns: Sequence[str],
      initial_event_types: Sequence[str],
      initial_bounds: TimeBounds | None,
      initial_layout: str,
      many_columns_threshold: int,
      title: str,
  ) -> None:
    super().__init__(
      loaded=loaded,
      loaded_events=loaded_events,
      initial_columns=initial_columns,
      initial_event_types=initial_event_types,
      initial_bounds=initial_bounds,
      initial_layout=initial_layout,
      many_columns_threshold=many_columns_threshold,
      title=title,
      browse_output=("Browse log…","Load debug logs"),
      output_label="Debug Log Output",
      loaded_label="Loaded prefix",
      loading_label_prefix="Debug log files",
    )

  def browse_for_output(self) -> None:
    current = self.output_edit.text().strip()
    if current:
      start_dir = str(Path(current).expanduser().resolve().parent)
    else:
      start_dir = str(Path.cwd())
    filename, _ = QFileDialog.getOpenFileName(
      self,
      "Select any debug log file to derive the prefix",
      start_dir,
      "Debug log files (*_envi.log *_fluxes.log *_trackers.log);;All files (*)",
    )
    if filename:
      self.output_edit.setText(derive_prefix_from_log_file(Path(filename)))

  @staticmethod
  def load_output(prefix: Path) -> LoadedSipnetData:
    """Load the three debug log files and merge them into a single LoadedSipnetData.

    File paths are derived by appending ``_envi.log``, ``_fluxes.log``, and
    ``_trackers.log`` to *prefix*.  Columns from each file are renamed with the
    source prefix (``envi.``, ``flux.``, ``tracker.``) so that the merged frame
    has unique, identifiable column names.
    """
    frames = {}
    for source, suffix in DEBUG_LOG_SOURCES.items():
      path = Path(str(prefix) + suffix)
      if not path.exists():
        fail(
          f"Debug log file not found: {path}. "
          f"Expected to find {prefix}_envi.log, {prefix}_fluxes.log, "
          f"and {prefix}_trackers.log."
        )

      header_row = find_output_header_row(path)
      frame = pd.read_csv(
        path,
        sep=r"\s+",
        engine="python",
        skiprows=header_row,
        header=0,
      )

      if frame.empty:
        fail(f"Debug log file {path} contains a header but no data rows.")

      # Rename non-time columns with source prefix to identify their origin.
      rename_map = {
        column: f"{source}.{column}"
        for column in frame.columns
        if column.lower() not in TIME_COLUMN_NAMES
      }
      frame = frame.rename(columns=rename_map)

      frames[source] = frame

    # Merge on year/day/time.  All three files cover the same timesteps.
    time_cols = list(TIME_COLUMN_NAMES)
    merged = frames["envi"]
    for source in ("flux", "tracker"):
      merged = merged.merge(frames[source], on=time_cols, how="inner")

    if merged.columns.duplicated().any():
      duplicates = merged.columns[merged.columns.duplicated()].tolist()
      fail(f"Duplicate column headers after merge: {duplicates}")

    lower_name_map = {column.lower(): column for column in merged.columns}
    missing = [name for name in TIME_COLUMN_NAMES if name not in lower_name_map]
    if missing:
      fail(f"Merged debug log is missing required time columns: {', '.join(missing)}")

    time_columns = tuple(lower_name_map[name] for name in TIME_COLUMN_NAMES)

    try:
      for column in merged.columns:
        merged[column] = pd.to_numeric(merged[column], errors="raise")
    except Exception as exc:
      fail(f"Failed to parse numeric data from debug log files at {prefix}: {exc}")

    plot_columns = [
      column for column in merged.columns if column not in time_columns
    ]
    if not plot_columns:
      fail(
        f"Debug log files at {prefix} have no plottable y-axis columns after excluding "
        f"{', '.join(time_columns)}."
      )

    timestamps: list = []
    year_column, day_column, time_column = time_columns
    for year_val, day_val, hour_val in zip(
        merged[year_column], merged[day_column], merged[time_column]
    ):
      timestamps.append(build_timestamp(year_val, day_val, hour_val))

    merged = merged.copy()  # de-fragment this monster

    merged[INTERNAL_TIMESTAMP_COLUMN] = timestamps
    merged = merged.sort_values(INTERNAL_TIMESTAMP_COLUMN, kind="stable").reset_index(drop=True)

    start_row = merged.iloc[0]
    end_row = merged.iloc[-1]
    full_start = start_row[INTERNAL_TIMESTAMP_COLUMN]
    full_end = end_row[INTERNAL_TIMESTAMP_COLUMN]

    full_start_label = format_time_value(
      int(start_row[year_column]),
      int(start_row[day_column]),
      float(start_row[time_column]),
    )
    full_end_label = format_time_value(
      int(end_row[year_column]),
      int(end_row[day_column]),
      float(end_row[time_column]),
    )

    return LoadedSipnetData(
      path=prefix,
      frame=merged,
      time_columns=time_columns,
      plot_columns=plot_columns,
      full_start=full_start,
      full_end=full_end,
      full_start_label=full_start_label,
      full_end_label=full_end_label,
    )

  def show_add_column_dialog(self) -> None:
    dialog = AddColumnDialog(
      self.create_new_column,
      self,
      expression_desc=(
        "Use backtick syntax for prefixed names, e.g. `envi.plantWoodC` + "
        "`flux.photosynthesis`"
      ),
    )
    if dialog.exec() == QDialog.Accepted:
      self.set_status(
        f"Added new column {dialog.column_name!r}. Click Apply to plot it.",
        is_error=False,
      )


def build_arg_parser() -> argparse.ArgumentParser:
  parser = get_default_arg_parser(
    "Interactive explorer for SIPNET debug log files "
    "(*_envi.log, *_fluxes.log, *_trackers.log)."
  )
  parser.add_argument(
    "-i",
    "--input-prefix",
    default="debug",
    help=(
      "Path/prefix for SIPNET debug log files. "
      "The tool appends _envi.log, _fluxes.log, and _trackers.log to this "
      "prefix. Defaults to ./debug; should match the prefix used with the "
      "--debug-log option of sipnet."
    ),
  )
  parser.add_argument(
    "--title",
    default="SIPNET Debug Log Viewer",
    help=(
      "Title displayed on the viewer window."
    ),
  )
  return parser


def main(argv: Sequence[str] | None = None) -> int:
  parser = build_arg_parser()
  argcomplete.autocomplete(parser)
  args = parser.parse_args(argv)

  prefix = Path(args.input_prefix).expanduser()
  loaded_output = SipnetDebugViewerWindow.load_output(prefix)

  requested_columns = split_csv_argument(args.columns)
  selected_columns = validate_requested_values(
    requested_columns,
    loaded_output.plot_columns,
    "columns",
  )

  initial_bounds = parse_time_range(args.time_range) if args.time_range else None
  loaded_events, selected_event_types = load_initial_events(args, loaded_output)

  app_argv = sys.argv if argv is None else [sys.argv[0], *argv]
  application = QApplication(app_argv)
  window = SipnetDebugViewerWindow(
    loaded=loaded_output,
    loaded_events=loaded_events,
    initial_columns=selected_columns,
    initial_event_types=selected_event_types,
    initial_bounds=initial_bounds,
    initial_layout=args.layout,
    many_columns_threshold=args.many_columns_threshold,
    title=args.title,
  )
  window.show()
  return application.exec()


def load_debug_tables(prefix: Path) -> LoadedSipnetData:
  """Load and merge the three debug log files at *prefix*. Exposes the static method at module level for backwards compatibility."""
  return SipnetDebugViewerWindow.load_output(prefix)


if __name__ == "__main__":
  try:
    raise SystemExit(main())
  except ValueError as exc:
    print(f"Error: {exc}", file=sys.stderr)
    raise SystemExit(2)

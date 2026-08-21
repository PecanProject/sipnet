#!/usr/bin/env python3
from __future__ import annotations

import argcomplete
import sys

from PySide6.QtWidgets import QApplication

from .sipnet_view_common import *


class SipnetViewerWindow(SipnetViewerWindowCore):
  def __init__(
      self,
      loaded_output: LoadedSipnetData,
      loaded_events: LoadedEventsData | None,
      initial_columns: Sequence[str],
      initial_event_types: Sequence[str],
      initial_bounds: TimeBounds | None,
      initial_layout: str,
      many_columns_threshold: int,
      title: str,
  ) -> None:
    super().__init__(
      loaded=loaded_output,
      loaded_events=loaded_events,
      initial_columns=initial_columns,
      initial_event_types=initial_event_types,
      initial_bounds=initial_bounds,
      initial_layout=initial_layout,
      many_columns_threshold=many_columns_threshold,
      title=title,
      browse_output=("Browse output…","Load output"),
      output_label="Main SIPNET Output",
      loaded_label="Loaded file",
      loading_label_prefix="Output file",
    )

  def browse_for_output(self) -> None:
    current = self.output_edit.text().strip() or str(self.loaded.path)
    start_dir = str(Path(current).expanduser().resolve().parent)
    filename, _ = QFileDialog.getOpenFileName(
      self,
      "Select SIPNET output file",
      start_dir,
      "Output files (*.out);;All files (*)",
    )
    if filename:
      self.output_edit.setText(filename)

  @staticmethod
  def load_output(path: Path) -> LoadedSipnetData:
    if path.is_dir():
      path = path / 'sipnet.out'

    if not path.exists():
      fail(f"Input file not found: {path}")

    header_row = find_output_header_row(path)
    frame = pd.read_csv(
      path,
      sep=r"\s+",
      engine="python",
      skiprows=header_row,
      header=0,
    )

    if frame.columns.duplicated().any():
      duplicates = frame.columns[frame.columns.duplicated()].tolist()
      fail(f"Duplicate column headers are not supported: {duplicates}")

    if frame.empty:
      fail(f"File {path} contains a header but no data rows.")

    lower_name_map = {column.lower(): column for column in frame.columns}
    missing = [name for name in TIME_COLUMN_NAMES if name not in lower_name_map]
    if missing:
      fail(f"Input file is missing required time columns: {', '.join(missing)}")

    time_columns = tuple(lower_name_map[name] for name in TIME_COLUMN_NAMES)

    try:
      for column in frame.columns:
        frame[column] = pd.to_numeric(frame[column], errors="raise")
    except Exception as exc:
      fail(f"Failed to parse numeric data from {path}: {exc}")

    plot_columns = [
      column for column in frame.columns if column not in time_columns
    ]
    if not plot_columns:
      fail(
        f"File {path} has no plottable y-axis columns after excluding "
        f"{', '.join(time_columns)}."
      )

    timestamps: list[datetime] = []
    year_column, day_column, time_column = time_columns
    for year_value, day_value, hour_value in zip(
        frame[year_column], frame[day_column], frame[time_column]
    ):
      timestamps.append(build_timestamp(year_value, day_value, hour_value))

    frame[INTERNAL_TIMESTAMP_COLUMN] = timestamps
    frame = frame.sort_values(INTERNAL_TIMESTAMP_COLUMN, kind="stable").reset_index(drop=True)

    start_row = frame.iloc[0]
    end_row = frame.iloc[-1]
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
      path=path,
      frame=frame,
      time_columns=time_columns,
      plot_columns=plot_columns,
      full_start=full_start,
      full_end=full_end,
      full_start_label=full_start_label,
      full_end_label=full_end_label,
    )

  def show_add_column_dialog(self) -> None:
    dialog = AddColumnDialog(self.create_new_column, self)
    if dialog.exec() == QDialog.Accepted:
      self.set_status(
        f"Added new column {dialog.column_name!r}. Click Apply to plot it.",
        is_error=False,
      )


def build_arg_parser() -> argparse.ArgumentParser:
  parser = get_default_arg_parser(
    "Interactive explorer for SIPNET output and events files."
  )
  parser.add_argument(
    "-i",
    "--input-file",
    default="sipnet.out",
    help= (
      "Path to a SIPNET output file. Defaults to ./sipnet.out. If directory, "
      "looks for sipnet.out there."
    )
  )
  parser.add_argument(
    "--title",
    default="SIPNET Output Viewer",
    help=(
      "Title displayed on the viewer window."
    ),
  )
  return parser


def main(argv: Sequence[str] | None = None) -> int:
  parser = build_arg_parser()
  argcomplete.autocomplete(parser)
  args = parser.parse_args(argv)

  input_path = Path(args.input_file).expanduser()
  loaded_output = SipnetViewerWindow.load_output(input_path)

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
  window = SipnetViewerWindow(
    loaded_output=loaded_output,
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


def load_output_table(path: Path) -> LoadedSipnetData:
  """Load the output table from *path*. Exposes the static method at module level for backwards compatibility."""
  return SipnetViewerWindow.load_output(path)


if __name__ == "__main__":
  try:
    raise SystemExit(main())
  except ValueError as exc:
    print(f"Error: {exc}", file=sys.stderr)
    raise SystemExit(2)

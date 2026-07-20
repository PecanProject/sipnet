#!/usr/bin/env python3
from __future__ import annotations

import argcomplete, argparse
import sys
from pathlib import Path
from typing import Sequence

import matplotlib.dates as mdates
import pandas as pd
from matplotlib.backends.backend_qtagg import FigureCanvasQTAgg, NavigationToolbar2QT
from matplotlib.figure import Figure
from matplotlib.lines import Line2D
from PySide6.QtWidgets import (
  QApplication,
  QAbstractItemView,
  QComboBox,
  QDialog,
  QFileDialog,
  QFormLayout,
  QHBoxLayout,
  QLabel,
  QLineEdit,
  QListWidget,
  QMainWindow,
  QPushButton,
  QVBoxLayout,
  QWidget,
)

from sipnet_view import (
  INTERNAL_TIMESTAMP_COLUMN,
  PLOT_COLOR_CYCLE,
  TIME_COLUMN_NAMES,
  AddColumnDialog,
  LoadedSipnetData,
  TimeBounds,
  add_derived_column,
  build_timestamp,
  fail,
  find_output_header_row,
  format_time_value,
  parse_time_point,
  parse_time_range,
  split_csv_argument,
  validate_requested_values,
)

DEBUG_LOG_SOURCES = {
  "envi": "_envi.log",
  "flux": "_fluxes.log",
  "tracker": "_trackers.log",
}


def load_debug_tables(prefix: Path) -> LoadedSipnetData:
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


def derive_prefix_from_log_file(log_path: Path) -> str:
  """Return the debug log prefix by stripping a known log suffix from *log_path*."""
  name = str(log_path)
  for suffix in DEBUG_LOG_SOURCES.values():
    if name.endswith(suffix):
      return name[: -len(suffix)]
  return name


class SipnetDebugViewerWindow(QMainWindow):
  def __init__(
      self,
      loaded: LoadedSipnetData,
      initial_columns: Sequence[str],
      initial_bounds: TimeBounds | None,
      initial_layout: str,
      many_columns_threshold: int,
  ) -> None:
    super().__init__()
    self.loaded = loaded
    self.many_columns_threshold = many_columns_threshold

    self.setWindowTitle("SIPNET Debug Log Viewer")
    self.resize(1500, 900)

    central_widget = QWidget(self)
    self.setCentralWidget(central_widget)
    outer_layout = QHBoxLayout(central_widget)

    control_widget = QWidget(self)
    control_layout = QVBoxLayout(control_widget)
    control_layout.setContentsMargins(0, 0, 0, 0)

    prefix_row = QHBoxLayout()
    self.prefix_edit = QLineEdit(str(self.loaded.path))
    self.prefix_browse_button = QPushButton("Browse log…")
    self.prefix_load_button = QPushButton("Load debug logs")
    prefix_row.addWidget(self.prefix_edit)
    prefix_row.addWidget(self.prefix_browse_button)
    prefix_row.addWidget(self.prefix_load_button)

    self.prefix_info_label = QLabel()
    self.prefix_info_label.setWordWrap(True)

    form_layout = QFormLayout()
    self.start_edit = QLineEdit()
    self.end_edit = QLineEdit()
    self.start_edit.setPlaceholderText("YYYY-DOY-HH")
    self.end_edit.setPlaceholderText("YYYY-DOY-HH")
    form_layout.addRow("Start time", self.start_edit)
    form_layout.addRow("End time", self.end_edit)

    self.layout_combo = QComboBox()
    self.layout_combo.addItem("Combined (twinned y-axes)", "combined")
    self.layout_combo.addItem("Split subplots", "subplots")
    combo_index = self.layout_combo.findData(initial_layout)
    if combo_index >= 0:
      self.layout_combo.setCurrentIndex(combo_index)
    form_layout.addRow("Layout", self.layout_combo)

    self.columns_list = QListWidget()
    self.columns_list.setSelectionMode(QAbstractItemView.MultiSelection)

    self.add_column_button = QPushButton("Add new column")
    self.apply_button = QPushButton("Apply")
    self.apply_button.setDefault(True)

    self.status_label = QLabel("Choose one or more columns, then click Apply.")
    self.status_label.setWordWrap(True)

    control_layout.addWidget(QLabel("Debug log prefix"))
    control_layout.addLayout(prefix_row)
    control_layout.addWidget(self.prefix_info_label)

    control_layout.addLayout(form_layout)
    columns_header = QHBoxLayout()
    columns_header.addWidget(QLabel("Y-axis columns"))
    columns_header.addStretch(1)
    columns_header.addWidget(self.add_column_button)
    control_layout.addLayout(columns_header)
    control_layout.addWidget(self.columns_list, stretch=1)
    control_layout.addWidget(self.apply_button)
    control_layout.addWidget(self.status_label)

    self.figure = Figure()
    self.canvas = FigureCanvasQTAgg(self.figure)
    self.toolbar = NavigationToolbar2QT(self.canvas, self)

    plot_widget = QWidget(self)
    plot_layout = QVBoxLayout(plot_widget)
    plot_layout.setContentsMargins(0, 0, 0, 0)
    plot_layout.addWidget(self.toolbar)
    plot_layout.addWidget(self.canvas)

    outer_layout.addWidget(control_widget, stretch=0)
    outer_layout.addWidget(plot_widget, stretch=1)

    self.prefix_browse_button.clicked.connect(self.browse_for_prefix)
    self.prefix_load_button.clicked.connect(self.load_selected_debug_files)
    self.add_column_button.clicked.connect(self.show_add_column_dialog)
    self.apply_button.clicked.connect(self.apply_view)

    self.populate_output_controls(initial_columns, initial_bounds)

  def set_status(self, message: str, is_error: bool = False) -> None:
    color = "#a40000" if is_error else "#1f4f7a"
    self.status_label.setStyleSheet(f"color: {color};")
    self.status_label.setText(message)

  def populate_output_controls(
      self,
      selected_columns: Sequence[str],
      time_bounds: TimeBounds | None,
  ) -> None:
    row_count = len(self.loaded.frame.index)
    column_count = len(self.loaded.plot_columns)
    self.prefix_info_label.setText(
      f"Loaded prefix: {self.loaded.path}\n"
      f"Rows: {row_count}\n"
      f"Plottable columns: {column_count}\n"
      f"Default time range: {self.loaded.full_start_label} to {self.loaded.full_end_label}"
    )

    if time_bounds is None:
      self.start_edit.setText(self.loaded.full_start_label)
      self.end_edit.setText(self.loaded.full_end_label)
    else:
      self.start_edit.setText(self.format_datetime_for_display(time_bounds.start))
      self.end_edit.setText(self.format_datetime_for_display(time_bounds.end))

    self.columns_list.clear()
    selected_set = set(selected_columns)
    for column in self.loaded.plot_columns:
      self.columns_list.addItem(column)
      item = self.columns_list.item(self.columns_list.count() - 1)
      if column in selected_set:
        item.setSelected(True)

  def browse_for_prefix(self) -> None:
    current = self.prefix_edit.text().strip()
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
      self.prefix_edit.setText(derive_prefix_from_log_file(Path(filename)))

  def load_selected_debug_files(self) -> None:
    requested_prefix = Path(self.prefix_edit.text().strip()).expanduser()
    try:
      loaded = load_debug_tables(requested_prefix)
    except Exception as exc:
      self.set_status(str(exc), is_error=True)
      return

    self.loaded = loaded
    self.prefix_edit.setText(str(self.loaded.path))
    self.populate_output_controls(selected_columns=[], time_bounds=None)
    self.figure.clear()
    self.canvas.draw_idle()
    self.set_status(
      "Debug log files loaded. Select columns and click Apply.",
      is_error=False,
    )

  def selected_plot_columns(self) -> list[str]:
    return [item.text() for item in self.columns_list.selectedItems()]

  def current_time_bounds(self) -> TimeBounds:
    start_text = self.start_edit.text().strip()
    end_text = self.end_edit.text().strip()

    start = self.loaded.full_start if not start_text else parse_time_point(start_text)
    end = self.loaded.full_end if not end_text else parse_time_point(end_text)

    if start > end:
      fail("Start time must be earlier than or equal to end time.")

    return TimeBounds(start=start, end=end)

  def format_datetime_for_display(self, value) -> str:
    year = value.year
    day = value.timetuple().tm_yday
    hour = (
        value.hour
        + value.minute / 60.0
        + value.second / 3600.0
        + value.microsecond / 3_600_000_000.0
    )
    return format_time_value(year, day, hour)

  def show_add_column_dialog(self) -> None:
    dialog = AddColumnDialog(self.create_new_column, self)
    if dialog.exec() == QDialog.Accepted:
      self.set_status(
        f"Added new column {dialog.column_name!r}. Click Apply to plot it.",
        is_error=False,
      )

  def create_new_column(self, name: str, expression: str) -> str:
    new_name = add_derived_column(self.loaded, name, expression)
    self.columns_list.addItem(new_name)
    item = self.columns_list.item(self.columns_list.count() - 1)
    item.setSelected(True)
    return new_name

  def apply_view(self) -> None:
    selected_columns = self.selected_plot_columns()
    if not selected_columns:
      self.set_status(
        "Select one or more y-axis columns, then click Apply.",
        is_error=False,
      )
      return

    try:
      bounds = self.current_time_bounds()
    except Exception as exc:
      self.set_status(str(exc), is_error=True)
      return

    filtered_output = self.loaded.frame.loc[
      (self.loaded.frame[INTERNAL_TIMESTAMP_COLUMN] >= bounds.start)
      & (self.loaded.frame[INTERNAL_TIMESTAMP_COLUMN] <= bounds.end)
      ]

    if filtered_output.empty:
      self.set_status("No rows fall within the selected time range.", is_error=True)
      return

    layout = self.layout_combo.currentData()
    if layout == "combined":
      self.plot_combined(filtered_output, selected_columns)
    else:
      self.plot_subplots(filtered_output, selected_columns)

    self.set_status(
      f"Plotted {len(selected_columns)} column(s) using {len(filtered_output)} rows.",
      is_error=False,
    )

  def plot_combined(
      self,
      frame: pd.DataFrame,
      columns: Sequence[str],
  ) -> None:
    self.figure.clear()

    right_margin = max(0.35, 0.90 - 0.08 * max(0, len(columns) - 1))
    self.figure.subplots_adjust(left=0.10, right=right_margin, bottom=0.15, top=0.90)

    base_axis = self.figure.add_subplot(111)
    x_values = frame[INTERNAL_TIMESTAMP_COLUMN]
    series_handles: list[Line2D] = []

    for index, column in enumerate(columns):
      color = PLOT_COLOR_CYCLE[index % len(PLOT_COLOR_CYCLE)]
      if index == 0:
        axis = base_axis
        axis.yaxis.set_label_position("left")
        axis.yaxis.tick_left()
      else:
        axis = base_axis.twinx()
        offset = 1.0 + 0.12 * (index - 1)
        axis.spines["right"].set_position(("axes", offset))
        axis.spines["right"].set_visible(True)

      line, = axis.plot(
        x_values,
        frame[column],
        label=column,
        color=color,
        linewidth=1.5,
        zorder=2,
      )
      axis.set_ylabel(column, color=color)
      axis.tick_params(axis="y", colors=color)
      series_handles.append(line)

    base_axis.set_title(f"{self.loaded.path.name} — combined view")
    base_axis.set_xlabel("Time")
    base_axis.grid(True, alpha=0.3)
    base_axis.xaxis.set_major_formatter(mdates.DateFormatter("%Y-%j\n%H:%M"))

    if series_handles:
      base_axis.legend(
        series_handles,
        [handle.get_label() for handle in series_handles],
        loc="upper left",
        fontsize="small",
        title="Series",
      )

    self.figure.autofmt_xdate()
    self.canvas.draw_idle()

  def plot_subplots(
      self,
      frame: pd.DataFrame,
      columns: Sequence[str],
  ) -> None:
    self.figure.clear()
    x_values = frame[INTERNAL_TIMESTAMP_COLUMN]
    axes = self.figure.subplots(len(columns), 1, sharex=True, squeeze=False)
    flat_axes = [axis for row in axes for axis in row]

    for index, (axis, column) in enumerate(zip(flat_axes, columns)):
      color = PLOT_COLOR_CYCLE[index % len(PLOT_COLOR_CYCLE)]
      axis.plot(x_values, frame[column], color=color, linewidth=1.5, zorder=2)
      axis.set_ylabel(column)
      axis.grid(True, alpha=0.3)

      if index == 0:
        axis.set_title(f"{self.loaded.path.name} — subplot view")

    flat_axes[-1].set_xlabel("Time")
    flat_axes[-1].xaxis.set_major_formatter(mdates.DateFormatter("%Y-%j\n%H:%M"))

    self.figure.tight_layout()
    self.figure.autofmt_xdate()
    self.canvas.draw_idle()


def build_arg_parser() -> argparse.ArgumentParser:
  parser = argparse.ArgumentParser(
    description=(
      "Interactive explorer for SIPNET debug log files "
      "(*_envi.log, *_fluxes.log, *_trackers.log)."
    )
  )
  parser.add_argument(
    "-i",
    "--input-prefix",
    default="sipnet_debug",
    help=(
      "Path/prefix for SIPNET debug log files. "
      "The tool appends _envi.log, _fluxes.log, and _trackers.log to this prefix. "
      "Defaults to ./sipnet_debug, matching the --debug-log option of sipnet."
    ),
  )
  parser.add_argument(
    "-t",
    "--time-range",
    help=(
      "Initial time range in the form "
      "YYYY-DOY-HH,YYYY-DOY-HH "
      "(example: 2016-001-00.00,2016-032-12.00)."
    ),
  )
  parser.add_argument(
    "-c",
    "--columns",
    help=(
      "Comma-separated list of columns to pre-select in the GUI. "
      "Use the prefixed names as shown in the Y-axis selector "
      "(e.g. envi.plantWoodC, flux.photosynthesis, tracker.t.gpp)."
    ),
  )
  parser.add_argument(
    "-l",
    "--layout",
    choices=("subplots", "combined"),
    default="subplots",
    help="Initial plot layout. 'combined' uses twinned y-axes.",
  )
  parser.add_argument(
    "--many-columns-threshold",
    type=int,
    default=6,
    help=(
      "Reserved for future use. Accepted for compatibility, "
      "but warnings are currently disabled."
    ),
  )
  return parser


def main(argv: Sequence[str] | None = None) -> int:
  parser = build_arg_parser()
  argcomplete.autocomplete(parser)
  args = parser.parse_args(argv)

  prefix = Path(args.input_prefix).expanduser()
  loaded = load_debug_tables(prefix)

  requested_columns = split_csv_argument(args.columns)
  selected_columns = validate_requested_values(
    requested_columns,
    loaded.plot_columns,
    "columns",
  )

  initial_bounds = parse_time_range(args.time_range) if args.time_range else None

  app_argv = sys.argv if argv is None else [sys.argv[0], *argv]
  application = QApplication(app_argv)
  window = SipnetDebugViewerWindow(
    loaded=loaded,
    initial_columns=selected_columns,
    initial_bounds=initial_bounds,
    initial_layout=args.layout,
    many_columns_threshold=args.many_columns_threshold,
  )
  window.show()
  return application.exec()


if __name__ == "__main__":
  try:
    raise SystemExit(main())
  except ValueError as exc:
    print(f"Error: {exc}", file=sys.stderr)
    raise SystemExit(2)

#!/usr/bin/env python3
from __future__ import annotations

import argparse
import math
import re
import sys
from dataclasses import dataclass
from datetime import datetime, timedelta
from pathlib import Path
from typing import Sequence

import matplotlib.dates as mdates
import pandas as pd
from matplotlib.backends.backend_qtagg import FigureCanvasQTAgg, NavigationToolbar2QT
from matplotlib.figure import Figure
from PySide6.QtWidgets import (
  QApplication,
  QAbstractItemView,
  QComboBox,
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

TIME_COLUMN_NAMES = ("year", "day", "time")
INTERNAL_TIMESTAMP_COLUMN = "__sipnet_timestamp__"
TIME_POINT_PATTERN = re.compile(
  r"^\s*(?P<year>\d{4})-(?P<day>\d{1,3})-(?P<hour>\d+(?:\.\d+)?)\s*$"
)


@dataclass(frozen=True)
class TimeBounds:
  start: datetime
  end: datetime


@dataclass
class LoadedSipnetData:
  path: Path
  frame: pd.DataFrame
  time_columns: tuple[str, str, str]
  plot_columns: list[str]
  full_start: datetime
  full_end: datetime
  full_start_label: str
  full_end_label: str


def fail(message: str) -> None:
  raise ValueError(message)


def max_day_of_year(year: int) -> int:
  return (datetime(year + 1, 1, 1) - datetime(year, 1, 1)).days


def format_time_value(year: int, day: int, hour: float) -> str:
  return f"{year:04d}-{day:03d}-{hour:05.2f}"


def is_numeric_token(token: str) -> bool:
  try:
    float(token)
    return True
  except ValueError:
    return False


def looks_like_data_line(tokens: list[str]) -> bool:
  if len(tokens) < 3:
    return False
  return all(is_numeric_token(token) for token in tokens[:3])


def build_timestamp(year_value: float, day_value: float, hour_value: float) -> datetime:
  if not float(year_value).is_integer():
    fail(f"Invalid year value {year_value!r}; expected an integer.")
  if not float(day_value).is_integer():
    fail(f"Invalid day value {day_value!r}; expected an integer.")

  year = int(year_value)
  day = int(day_value)
  hour = float(hour_value)

  if day < 1 or day > max_day_of_year(year):
    fail(
      f"Invalid day-of-year {day} for year {year}; expected 1..{max_day_of_year(year)}."
    )
  if hour < 0 or hour > 24:
    fail(f"Invalid hour value {hour!r}; expected 0 <= hour <= 24.")

  return datetime(year, 1, 1) + timedelta(days=day - 1, hours=hour)


def parse_time_point(text: str) -> datetime:
  match = TIME_POINT_PATTERN.fullmatch(text)
  if match is None:
    fail(
      f"Invalid time value {text!r}. Expected format YYYY-DOY-HH "
      f"(example: 2016-001-00.00)."
    )

  year = int(match.group("year"))
  day = int(match.group("day"))
  hour = float(match.group("hour"))
  return build_timestamp(year, day, hour)


def parse_time_range(text: str) -> TimeBounds:
  parts = [part.strip() for part in text.split(",")]
  if len(parts) != 2 or not parts[0] or not parts[1]:
    fail(
      "Invalid time range. Expected format "
      "'YYYY-DOY-HH,YYYY-DOY-HH' "
      "(example: 2016-001-00.00,2016-032-12.00)."
    )

  start = parse_time_point(parts[0])
  end = parse_time_point(parts[1])

  if start > end:
    fail("Invalid time range: start time must be earlier than or equal to end time.")

  return TimeBounds(start=start, end=end)


def split_columns_argument(text: str | None) -> list[str]:
  if text is None:
    return []
  columns = [column.strip() for column in text.split(",")]
  return [column for column in columns if column]


def find_header_row(path: Path) -> int:
  with path.open("r", encoding="utf-8") as handle:
    for row_index, line in enumerate(handle):
      stripped = line.strip()
      if not stripped:
        continue

      tokens = stripped.split()
      lowered = [token.lower() for token in tokens[:3]]

      if len(tokens) >= 3 and tuple(lowered) == TIME_COLUMN_NAMES:
        return row_index

      if looks_like_data_line(tokens):
        fail(
          f"File {path} does not contain a required header row before data. "
          f"Expected a header beginning with 'year day time'."
        )

  fail(
    f"File {path} does not contain a required header row. "
    f"Expected a line beginning with 'year day time'."
  )


def load_output_table(path: Path) -> LoadedSipnetData:
  if not path.exists():
    fail(f"Input file not found: {path}")

  header_row = find_header_row(path)
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

  plot_columns = [column for column in frame.columns if column not in time_columns]
  if not plot_columns:
    fail(
      f"File {path} has no plottable y-axis columns after excluding "
      f"{', '.join(time_columns)}."
    )

  timestamps: list[datetime] = []
  labels: list[str] = []

  year_column, day_column, time_column = time_columns
  for year_value, day_value, hour_value in zip(
      frame[year_column], frame[day_column], frame[time_column]
  ):
    timestamp = build_timestamp(year_value, day_value, hour_value)
    timestamps.append(timestamp)
    labels.append(
      format_time_value(int(year_value), int(day_value), float(hour_value))
    )

  frame[INTERNAL_TIMESTAMP_COLUMN] = timestamps
  frame = frame.sort_values(INTERNAL_TIMESTAMP_COLUMN, kind="stable").reset_index(drop=True)

  start_row = frame.iloc[0]
  end_row = frame.iloc[-1]

  full_start = frame.iloc[0][INTERNAL_TIMESTAMP_COLUMN]
  full_end = frame.iloc[-1][INTERNAL_TIMESTAMP_COLUMN]

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


def validate_requested_columns(requested: Sequence[str], available: Sequence[str]) -> list[str]:
  available_set = set(available)
  unknown = [column for column in requested if column not in available_set]
  if unknown:
    fail(
      "Unknown columns requested via --columns: "
      + ", ".join(unknown)
      + ". Available columns: "
      + ", ".join(available)
    )
  return list(requested)


def build_arg_parser() -> argparse.ArgumentParser:
  parser = argparse.ArgumentParser(
    description="Interactive explorer for SIPNET output files."
  )
  parser.add_argument(
    "-i",
    "--input-file",
    default="sipnet.out",
    help="Path to a SIPNET output file. Defaults to ./sipnet.out",
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
    help="Comma-separated list of columns to pre-select in the GUI.",
  )
  parser.add_argument(
    "-l",
    "--layout",
    choices=("combined", "subplots"),
    default="combined",
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


class SipnetViewerWindow(QMainWindow):
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
    self.initial_columns = list(initial_columns)
    self.many_columns_threshold = many_columns_threshold

    self.setWindowTitle("SIPNET Output Viewer")
    self.resize(1400, 850)

    central_widget = QWidget(self)
    self.setCentralWidget(central_widget)

    outer_layout = QHBoxLayout(central_widget)

    control_widget = QWidget(self)
    control_layout = QVBoxLayout(control_widget)
    control_layout.setContentsMargins(0, 0, 0, 0)

    file_row = QHBoxLayout()
    self.file_edit = QLineEdit(str(self.loaded.path))
    self.browse_button = QPushButton("Browse…")
    self.load_button = QPushButton("Load")
    file_row.addWidget(self.file_edit)
    file_row.addWidget(self.browse_button)
    file_row.addWidget(self.load_button)

    self.info_label = QLabel()
    self.info_label.setWordWrap(True)

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

    self.apply_button = QPushButton("Apply")
    self.apply_button.setDefault(True)

    self.status_label = QLabel("Choose one or more columns, then click Apply.")
    self.status_label.setWordWrap(True)

    control_layout.addLayout(file_row)
    control_layout.addWidget(self.info_label)
    control_layout.addLayout(form_layout)
    control_layout.addWidget(QLabel("Y-axis columns"))
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

    self.browse_button.clicked.connect(self.browse_for_file)
    self.load_button.clicked.connect(self.load_selected_file)
    self.apply_button.clicked.connect(self.apply_view)

    self.populate_controls(initial_columns, initial_bounds)

  def set_status(self, message: str, is_error: bool = False) -> None:
    color = "#a40000" if is_error else "#1f4f7a"
    self.status_label.setText(f"<span style='color:{color}'>{message}</span>")

  def populate_controls(
      self,
      selected_columns: Sequence[str],
      time_bounds: TimeBounds | None,
  ) -> None:
    row_count = len(self.loaded.frame.index)
    column_count = len(self.loaded.plot_columns)
    self.info_label.setText(
      f"Loaded file: {self.loaded.path.name}\n"
      f"Rows: {row_count}\n"
      f"Plottable columns: {column_count}\n"
      f"Default time range: {self.loaded.full_start_label} to {self.loaded.full_end_label}"
    )

    if time_bounds is None:
      self.start_edit.setText(self.loaded.full_start_label)
      self.end_edit.setText(self.loaded.full_end_label)
    else:
      self.start_edit.setText(
        self.format_datetime_for_display(time_bounds.start)
      )
      self.end_edit.setText(
        self.format_datetime_for_display(time_bounds.end)
      )

    self.columns_list.clear()
    selected_set = set(selected_columns)
    for column in self.loaded.plot_columns:
      self.columns_list.addItem(column)
      if column in selected_set:
        self.columns_list.item(self.columns_list.count() - 1).setSelected(True)

  def format_datetime_for_display(self, value: datetime) -> str:
    year = value.year
    day = value.timetuple().tm_yday
    hour = (
        value.hour
        + value.minute / 60.0
        + value.second / 3600.0
        + value.microsecond / 3_600_000_000.0
    )
    return format_time_value(year, day, hour)

  def browse_for_file(self) -> None:
    path_text = self.file_edit.text().strip() or str(self.loaded.path)
    start_dir = str(Path(path_text).expanduser().resolve().parent)
    filename, _ = QFileDialog.getOpenFileName(
      self,
      "Select SIPNET output file",
      start_dir,
      "Output files (*.out);;All files (*)",
    )
    if filename:
      self.file_edit.setText(filename)

  def load_selected_file(self) -> None:
    requested_path = Path(self.file_edit.text().strip()).expanduser()
    try:
      loaded = load_output_table(requested_path)
    except Exception as exc:
      self.set_status(str(exc), is_error=True)
      return

    self.loaded = loaded
    self.populate_controls(selected_columns=[], time_bounds=None)
    self.figure.clear()
    self.canvas.draw_idle()
    self.set_status(
      "File loaded. Select one or more columns, then click Apply.",
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

    filtered = self.loaded.frame.loc[
      (self.loaded.frame[INTERNAL_TIMESTAMP_COLUMN] >= bounds.start)
      & (self.loaded.frame[INTERNAL_TIMESTAMP_COLUMN] <= bounds.end)
      ]

    if filtered.empty:
      self.set_status(
        "No rows fall within the selected time range.",
        is_error=True,
      )
      return

    layout = self.layout_combo.currentData()
    if layout == "combined":
      self.plot_combined(filtered, selected_columns)
    else:
      self.plot_subplots(filtered, selected_columns)

    self.set_status(
      f"Plotted {len(selected_columns)} column(s) from "
      f"{self.loaded.path.name} using {len(filtered)} rows.",
      is_error=False,
    )

  def plot_combined(self, frame: pd.DataFrame, columns: Sequence[str]) -> None:
    self.figure.clear()

    right_margin = max(0.35, 0.90 - 0.08 * max(0, len(columns) - 1))
    self.figure.subplots_adjust(left=0.10, right=right_margin, bottom=0.15, top=0.90)

    base_axis = self.figure.add_subplot(111)
    x_values = frame[INTERNAL_TIMESTAMP_COLUMN]

    color_cycle = [
      "#1f77b4",
      "#d62728",
      "#2ca02c",
      "#9467bd",
      "#ff7f0e",
      "#8c564b",
      "#e377c2",
      "#7f7f7f",
      "#bcbd22",
      "#17becf",
    ]

    lines = []

    for index, column in enumerate(columns):
      color = color_cycle[index % len(color_cycle)]
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
      )
      axis.set_ylabel(column, color=color)
      axis.tick_params(axis="y", colors=color)
      lines.append(line)

    base_axis.set_title(f"{self.loaded.path.name} — combined view")
    base_axis.set_xlabel("Time")
    base_axis.grid(True, alpha=0.3)
    base_axis.xaxis.set_major_formatter(mdates.DateFormatter("%Y-%j\n%H:%M"))
    base_axis.legend(
      lines,
      [line.get_label() for line in lines],
      loc="upper left",
      fontsize="small",
    )
    self.figure.autofmt_xdate()
    self.canvas.draw_idle()

  def plot_subplots(self, frame: pd.DataFrame, columns: Sequence[str]) -> None:
    self.figure.clear()
    x_values = frame[INTERNAL_TIMESTAMP_COLUMN]
    axes = self.figure.subplots(len(columns), 1, sharex=True, squeeze=False)
    flat_axes = [axis for row in axes for axis in row]

    color_cycle = [
      "#1f77b4",
      "#d62728",
      "#2ca02c",
      "#9467bd",
      "#ff7f0e",
      "#8c564b",
      "#e377c2",
      "#7f7f7f",
      "#bcbd22",
      "#17becf",
    ]

    for index, (axis, column) in enumerate(zip(flat_axes, columns)):
      color = color_cycle[index % len(color_cycle)]
      axis.plot(x_values, frame[column], color=color, linewidth=1.5)
      axis.set_ylabel(column)
      axis.grid(True, alpha=0.3)
      if index == 0:
        axis.set_title(f"{self.loaded.path.name} — subplot view")

    flat_axes[-1].set_xlabel("Time")
    flat_axes[-1].xaxis.set_major_formatter(mdates.DateFormatter("%Y-%j\n%H:%M"))
    self.figure.tight_layout()
    self.figure.autofmt_xdate()
    self.canvas.draw_idle()


def main(argv: Sequence[str] | None = None) -> int:
  parser = build_arg_parser()
  args = parser.parse_args(argv)

  input_path = Path(args.input_file).expanduser()
  loaded = load_output_table(input_path)

  requested_columns = split_columns_argument(args.columns)
  validated_columns = validate_requested_columns(requested_columns, loaded.plot_columns)

  initial_bounds = parse_time_range(args.time_range) if args.time_range else None

  application = QApplication(sys.argv if argv is None else [sys.argv[0], *argv])
  window = SipnetViewerWindow(
    loaded=loaded,
    initial_columns=validated_columns,
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

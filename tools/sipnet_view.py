#!/usr/bin/env python3
from __future__ import annotations

import argparse
import sys
import re
from dataclasses import dataclass
from datetime import datetime, timedelta
from pathlib import Path
from typing import Sequence

import matplotlib.dates as mdates
import pandas as pd
from matplotlib import colormaps
from matplotlib.backends.backend_qtagg import FigureCanvasQTAgg, NavigationToolbar2QT
from matplotlib.figure import Figure
from matplotlib.lines import Line2D
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
EVENT_COLUMN_NAMES = ("year", "day", "type")

INTERNAL_TIMESTAMP_COLUMN = "sipnet_timestamp__"
EVENT_TIMESTAMP_COLUMN = "event_display_timestamp__"
EVENT_DAY_START_COLUMN = "event_day_start__"
EVENT_DAY_END_COLUMN = "event_day_end__"

TIME_POINT_PATTERN = re.compile(
  r"^\s*(?P<year>\d{4})-(?P<day>\d{1,3})-(?P<hour>\d+(?:\.\d+)?)\s*$"
)

PLOT_COLOR_CYCLE = [
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


@dataclass
class LoadedEventsData:
  path: Path
  frame: pd.DataFrame
  event_types: list[str]
  event_colors: dict[str, tuple[float, float, float, float]]


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
      f"Invalid day-of-year {day} for year {year}; "
      f"expected 1..{max_day_of_year(year)}."
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


def split_csv_argument(text: str | None) -> list[str]:
  if text is None:
    return []
  values = [value.strip() for value in text.split(",")]
  return [value for value in values if value]


def default_events_path(output_path: Path) -> Path:
  return output_path.with_name("events.out")


def looks_like_output_data_line(tokens: list[str]) -> bool:
  return len(tokens) >= 3 and all(is_numeric_token(token) for token in tokens[:3])


def looks_like_event_data_line(tokens: list[str]) -> bool:
  return (
      len(tokens) >= 3
      and is_numeric_token(tokens[0])
      and is_numeric_token(tokens[1])
  )


def find_output_header_row(path: Path) -> int:
  with path.open("r", encoding="utf-8") as handle:
    for row_index, line in enumerate(handle):
      stripped = line.strip()
      if not stripped:
        continue

      tokens = stripped.split()
      lowered = tuple(token.lower() for token in tokens[:3])

      if len(tokens) >= 3 and lowered == TIME_COLUMN_NAMES:
        return row_index

      if looks_like_output_data_line(tokens):
        fail(
          f"File {path} does not contain a required header row before data. "
          f"Expected a header beginning with 'year day time'."
        )

  fail(
    f"File {path} does not contain a required header row. "
    f"Expected a line beginning with 'year day time'."
  )


def find_events_header_row(path: Path) -> int:
  with path.open("r", encoding="utf-8") as handle:
    for row_index, line in enumerate(handle):
      stripped = line.strip()
      if not stripped:
        continue

      tokens = stripped.split()
      lowered = tuple(token.lower() for token in tokens[:3])

      if len(tokens) >= 3 and lowered == EVENT_COLUMN_NAMES:
        return row_index

      if looks_like_event_data_line(tokens):
        fail(
          f"File {path} does not contain a required event header row before data. "
          f"Expected a header beginning with 'year day type'."
        )

  fail(
    f"File {path} does not contain a required event header row. "
    f"Expected a line beginning with 'year day type'."
  )


def load_output_table(path: Path) -> LoadedSipnetData:
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

  plot_columns = [column for column in frame.columns if column not in time_columns]
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


def load_events_table(path: Path, output_data: LoadedSipnetData) -> LoadedEventsData:
  if not path.exists():
    fail(f"Events file not found: {path}")

  header_row = find_events_header_row(path)
  frame = pd.read_csv(
    path,
    sep=r"\s+",
    engine="python",
    skiprows=header_row,
    header=0,
    usecols=[0, 1, 2],
  )

  if frame.empty:
    fail(f"Events file {path} contains a header but no data rows.")

  actual_headers = tuple(str(column).strip().lower() for column in frame.columns[:3])
  if actual_headers != EVENT_COLUMN_NAMES:
    fail(
      f"Events file {path} must begin with columns "
      f"'year day type'; found {', '.join(map(str, frame.columns[:3]))}."
    )

  frame.columns = list(EVENT_COLUMN_NAMES)

  try:
    frame["year"] = pd.to_numeric(frame["year"], errors="raise")
    frame["day"] = pd.to_numeric(frame["day"], errors="raise")
  except Exception as exc:
    fail(f"Failed to parse year/day from events file {path}: {exc}")

  frame["type"] = frame["type"].astype(str).str.strip()
  if (frame["type"] == "").any():
    fail(f"Events file {path} contains an empty event type.")

  year_column, day_column, _ = output_data.time_columns
  day_to_first_timestamp: dict[tuple[int, int], datetime] = {}
  for year_value, day_value, timestamp in zip(
      output_data.frame[year_column],
      output_data.frame[day_column],
      output_data.frame[INTERNAL_TIMESTAMP_COLUMN],
  ):
    key = (int(year_value), int(day_value))
    if key not in day_to_first_timestamp:
      day_to_first_timestamp[key] = timestamp

  display_timestamps: list[datetime] = []
  day_starts: list[datetime] = []
  day_ends: list[datetime] = []

  for year_value, day_value in zip(frame["year"], frame["day"]):
    day_start = build_timestamp(year_value, day_value, 0.0)
    display_timestamp = day_to_first_timestamp.get(
      (int(year_value), int(day_value)),
      day_start,
    )
    display_timestamps.append(display_timestamp)
    day_starts.append(day_start)
    day_ends.append(day_start + timedelta(days=1) - timedelta(microseconds=1))

  frame[EVENT_TIMESTAMP_COLUMN] = display_timestamps
  frame[EVENT_DAY_START_COLUMN] = day_starts
  frame[EVENT_DAY_END_COLUMN] = day_ends
  frame = frame.sort_values(EVENT_TIMESTAMP_COLUMN, kind="stable").reset_index(drop=True)

  event_types = list(dict.fromkeys(frame["type"].tolist()))
  cmap = colormaps.get_cmap("tab20")
  denominator = max(1, len(event_types) - 1)
  event_colors = {
    event_type: cmap(index / denominator)
    for index, event_type in enumerate(event_types)
  }

  return LoadedEventsData(
    path=path,
    frame=frame,
    event_types=event_types,
    event_colors=event_colors,
  )


def validate_requested_values(
    requested: Sequence[str],
    available: Sequence[str],
    label: str,
) -> list[str]:
  available_set = set(available)
  unknown = [value for value in requested if value not in available_set]
  if unknown:
    fail(
      f"Unknown {label} requested: "
      + ", ".join(unknown)
      + f". Available {label}: "
      + ", ".join(available)
    )
  return list(requested)


def build_arg_parser() -> argparse.ArgumentParser:
  parser = argparse.ArgumentParser(
    description="Interactive explorer for SIPNET output and events files."
  )
  parser.add_argument(
    "-i",
    "--input-file",
    default="sipnet.out",
    help="Path to a SIPNET output file. Defaults to ./sipnet.out",
  )
  parser.add_argument(
    "--events-file",
    help="Optional path to an events output file. Defaults to events.out beside the main output.",
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
    help="Comma-separated list of SIPNET output columns to pre-select in the GUI.",
  )
  parser.add_argument(
    "--event-types",
    help="Comma-separated list of event types to pre-select in the GUI.",
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
      loaded_output: LoadedSipnetData,
      loaded_events: LoadedEventsData | None,
      initial_columns: Sequence[str],
      initial_event_types: Sequence[str],
      initial_bounds: TimeBounds | None,
      initial_layout: str,
      many_columns_threshold: int,
  ) -> None:
    super().__init__()
    self.loaded = loaded_output
    self.loaded_events = loaded_events
    self.many_columns_threshold = many_columns_threshold

    self.setWindowTitle("SIPNET Output Viewer")
    self.resize(1500, 900)

    central_widget = QWidget(self)
    self.setCentralWidget(central_widget)
    outer_layout = QHBoxLayout(central_widget)

    control_widget = QWidget(self)
    control_layout = QVBoxLayout(control_widget)
    control_layout.setContentsMargins(0, 0, 0, 0)

    output_row = QHBoxLayout()
    self.output_edit = QLineEdit(str(self.loaded.path))
    self.output_browse_button = QPushButton("Browse output…")
    self.output_load_button = QPushButton("Load output")
    output_row.addWidget(self.output_edit)
    output_row.addWidget(self.output_browse_button)
    output_row.addWidget(self.output_load_button)

    self.output_info_label = QLabel()
    self.output_info_label.setWordWrap(True)

    events_row = QHBoxLayout()
    initial_events_path = (
      str(self.loaded_events.path)
      if self.loaded_events is not None
      else str(default_events_path(self.loaded.path))
    )
    self.events_edit = QLineEdit(initial_events_path)
    self.events_browse_button = QPushButton("Browse events…")
    self.events_load_button = QPushButton("Load events")
    events_row.addWidget(self.events_edit)
    events_row.addWidget(self.events_browse_button)
    events_row.addWidget(self.events_load_button)

    self.events_info_label = QLabel()
    self.events_info_label.setWordWrap(True)

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

    self.event_types_list = QListWidget()
    self.event_types_list.setSelectionMode(QAbstractItemView.MultiSelection)

    self.apply_button = QPushButton("Apply")
    self.apply_button.setDefault(True)

    self.status_label = QLabel("Choose one or more columns, then click Apply.")
    self.status_label.setWordWrap(True)

    control_layout.addWidget(QLabel("Main SIPNET output"))
    control_layout.addLayout(output_row)
    control_layout.addWidget(self.output_info_label)

    control_layout.addWidget(QLabel("Events output"))
    control_layout.addLayout(events_row)
    control_layout.addWidget(self.events_info_label)

    control_layout.addLayout(form_layout)
    control_layout.addWidget(QLabel("Y-axis columns"))
    control_layout.addWidget(self.columns_list, stretch=1)
    control_layout.addWidget(QLabel("Event types"))
    control_layout.addWidget(self.event_types_list, stretch=0)
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

    self.output_browse_button.clicked.connect(self.browse_for_output_file)
    self.output_load_button.clicked.connect(self.load_selected_output_file)
    self.events_browse_button.clicked.connect(self.browse_for_events_file)
    self.events_load_button.clicked.connect(self.load_selected_events_file)
    self.apply_button.clicked.connect(self.apply_view)

    self.populate_output_controls(initial_columns, initial_bounds)
    self.populate_event_controls(initial_event_types)

  def set_status(self, message: str, is_error: bool = False) -> None:
    color = "#a40000" if is_error else "#1f4f7a"
    self.status_label.setText(f"<span style='color:{color}'>{message}</span>")

  def populate_output_controls(
      self,
      selected_columns: Sequence[str],
      time_bounds: TimeBounds | None,
  ) -> None:
    row_count = len(self.loaded.frame.index)
    column_count = len(self.loaded.plot_columns)
    self.output_info_label.setText(
      f"Loaded file: {self.loaded.path.name}\n"
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

  def populate_event_controls(self, selected_event_types: Sequence[str]) -> None:
    self.event_types_list.clear()

    if self.loaded_events is None:
      requested_path = Path(self.events_edit.text().strip()) if self.events_edit.text().strip() else default_events_path(self.loaded.path)
      self.events_info_label.setText(
        f"No events file loaded.\n"
        f"Default path: {requested_path}"
      )
      return

    self.events_info_label.setText(
      f"Loaded file: {self.loaded_events.path.name}\n"
      f"Events: {len(self.loaded_events.frame.index)}\n"
      f"Event types found: {', '.join(self.loaded_events.event_types)}"
    )

    selected_set = set(selected_event_types)
    for event_type in self.loaded_events.event_types:
      self.event_types_list.addItem(event_type)
      item = self.event_types_list.item(self.event_types_list.count() - 1)
      if event_type in selected_set:
        item.setSelected(True)

    self.event_types_list.setSortingEnabled(True)
    self.event_types_list.sortItems()
    
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

  def browse_for_output_file(self) -> None:
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

  def browse_for_events_file(self) -> None:
    current = self.events_edit.text().strip() or str(default_events_path(self.loaded.path))
    start_dir = str(Path(current).expanduser().resolve().parent)
    filename, _ = QFileDialog.getOpenFileName(
      self,
      "Select events output file",
      start_dir,
      "Output files (*.out);;All files (*)",
    )
    if filename:
      self.events_edit.setText(filename)

  def load_selected_output_file(self) -> None:
    requested_path = Path(self.output_edit.text().strip()).expanduser()
    try:
      loaded_output = load_output_table(requested_path)
    except Exception as exc:
      self.set_status(str(exc), is_error=True)
      return

    self.loaded = loaded_output
    self.output_edit.setText(str(self.loaded.path))
    self.populate_output_controls(selected_columns=[], time_bounds=None)

    default_path = default_events_path(self.loaded.path)
    self.events_edit.setText(str(default_path))

    if default_path.exists():
      try:
        self.loaded_events = load_events_table(default_path, self.loaded)
      except Exception as exc:
        self.loaded_events = None
        self.populate_event_controls(selected_event_types=[])
        self.figure.clear()
        self.canvas.draw_idle()
        self.set_status(
          f"Output loaded, but default events file could not be loaded: {exc}",
          is_error=True,
        )
        return

      self.populate_event_controls(selected_event_types=[])
      message = (
        "Output file loaded. Default events file loaded. "
        "Select columns and optional event types, then click Apply."
      )
    else:
      self.loaded_events = None
      self.populate_event_controls(selected_event_types=[])
      message = (
        "Output file loaded. No sibling events.out was found. "
        "Select columns and click Apply, or load an events file."
      )

    self.figure.clear()
    self.canvas.draw_idle()
    self.set_status(message, is_error=False)

  def load_selected_events_file(self) -> None:
    path_text = self.events_edit.text().strip()
    if not path_text:
      self.loaded_events = None
      self.populate_event_controls(selected_event_types=[])
      self.set_status("Events file cleared. No events will be shown.", is_error=False)
      return

    requested_path = Path(path_text).expanduser()
    try:
      self.loaded_events = load_events_table(requested_path, self.loaded)
    except Exception as exc:
      self.set_status(str(exc), is_error=True)
      return

    self.events_edit.setText(str(self.loaded_events.path))
    self.populate_event_controls(selected_event_types=[])
    self.set_status(
      "Events file loaded. Select event types and click Apply to show them.",
      is_error=False,
    )

  def selected_plot_columns(self) -> list[str]:
    return [item.text() for item in self.columns_list.selectedItems()]

  def selected_event_types(self) -> list[str]:
    return [item.text() for item in self.event_types_list.selectedItems()]

  def current_time_bounds(self) -> TimeBounds:
    start_text = self.start_edit.text().strip()
    end_text = self.end_edit.text().strip()

    start = self.loaded.full_start if not start_text else parse_time_point(start_text)
    end = self.loaded.full_end if not end_text else parse_time_point(end_text)

    if start > end:
      fail("Start time must be earlier than or equal to end time.")

    return TimeBounds(start=start, end=end)

  def filtered_events(self, bounds: TimeBounds, event_types: Sequence[str]) -> pd.DataFrame | None:
    if self.loaded_events is None or not event_types:
      return None

    filtered = self.loaded_events.frame.loc[
      self.loaded_events.frame["type"].isin(event_types)
      & (self.loaded_events.frame[EVENT_DAY_START_COLUMN] <= bounds.end)
      & (self.loaded_events.frame[EVENT_DAY_END_COLUMN] >= bounds.start)
      ]
    return filtered.reset_index(drop=True)

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

    selected_event_types = self.selected_event_types()
    filtered_events = self.filtered_events(bounds, selected_event_types)

    layout = self.layout_combo.currentData()
    if layout == "combined":
      self.plot_combined(filtered_output, selected_columns, filtered_events)
    else:
      self.plot_subplots(filtered_output, selected_columns, filtered_events)

    event_count = 0 if filtered_events is None else len(filtered_events.index)
    if selected_event_types:
      self.set_status(
        f"Plotted {len(selected_columns)} column(s) using {len(filtered_output)} rows "
        f"and displayed {event_count} event line(s).",
        is_error=False,
      )
    else:
      self.set_status(
        f"Plotted {len(selected_columns)} column(s) using {len(filtered_output)} rows. "
        f"No event types selected.",
        is_error=False,
      )

  def overlay_events_on_axis(self, axis, events: pd.DataFrame | None) -> list[Line2D]:
    if events is None or events.empty or self.loaded_events is None:
      return []

    for event in events.itertuples(index=False):
      axis.axvline(
        getattr(event, EVENT_TIMESTAMP_COLUMN),
        color=self.loaded_events.event_colors[event.type],
        linestyle="--",
        linewidth=1.2,
        alpha=0.85,
        zorder=1,
      )

    shown_types = [event_type for event_type in self.loaded_events.event_types if event_type in set(events["type"])]
    return [
      Line2D(
        [0],
        [0],
        color=self.loaded_events.event_colors[event_type],
        linestyle="--",
        linewidth=1.5,
        label=event_type,
      )
      for event_type in shown_types
    ]

  def plot_combined(
      self,
      frame: pd.DataFrame,
      columns: Sequence[str],
      events: pd.DataFrame | None,
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

    event_handles = self.overlay_events_on_axis(base_axis, events)

    base_axis.set_title(f"{self.loaded.path.name} — combined view")
    base_axis.set_xlabel("Time")
    base_axis.grid(True, alpha=0.3)
    base_axis.xaxis.set_major_formatter(mdates.DateFormatter("%Y-%j\n%H:%M"))

    if series_handles:
      series_legend = base_axis.legend(
        series_handles,
        [handle.get_label() for handle in series_handles],
        loc="upper left",
        fontsize="small",
        title="Series",
      )
      base_axis.add_artist(series_legend)

    if event_handles:
      base_axis.legend(
        handles=event_handles,
        loc="upper right",
        fontsize="small",
        title="Events",
      )

    self.figure.autofmt_xdate()
    self.canvas.draw_idle()

  def plot_subplots(
      self,
      frame: pd.DataFrame,
      columns: Sequence[str],
      events: pd.DataFrame | None,
  ) -> None:
    self.figure.clear()
    x_values = frame[INTERNAL_TIMESTAMP_COLUMN]
    axes = self.figure.subplots(len(columns), 1, sharex=True, squeeze=False)
    flat_axes = [axis for row in axes for axis in row]

    event_handles: list[Line2D] = []
    for index, (axis, column) in enumerate(zip(flat_axes, columns)):
      color = PLOT_COLOR_CYCLE[index % len(PLOT_COLOR_CYCLE)]
      axis.plot(x_values, frame[column], color=color, linewidth=1.5, zorder=2)
      axis.set_ylabel(column)
      axis.grid(True, alpha=0.3)

      current_event_handles = self.overlay_events_on_axis(axis, events)
      if current_event_handles and not event_handles:
        event_handles = current_event_handles

      if index == 0:
        axis.set_title(f"{self.loaded.path.name} — subplot view")

    flat_axes[-1].set_xlabel("Time")
    flat_axes[-1].xaxis.set_major_formatter(mdates.DateFormatter("%Y-%j\n%H:%M"))

    if event_handles:
      flat_axes[0].legend(
        handles=event_handles,
        loc="upper right",
        fontsize="small",
        title="Events",
      )

    self.figure.tight_layout()
    self.figure.autofmt_xdate()
    self.canvas.draw_idle()


def load_initial_events(
    args: argparse.Namespace,
    loaded_output: LoadedSipnetData,
) -> tuple[LoadedEventsData | None, list[str]]:
  requested_event_types = split_csv_argument(args.event_types)

  if args.events_file:
    events_path = Path(args.events_file).expanduser()
    loaded_events = load_events_table(events_path, loaded_output)
    selected_event_types = validate_requested_values(
      requested_event_types,
      loaded_events.event_types,
      "event types",
    )
    return loaded_events, selected_event_types

  default_path = default_events_path(loaded_output.path)
  if default_path.exists():
    loaded_events = load_events_table(default_path, loaded_output)
    selected_event_types = validate_requested_values(
      requested_event_types,
      loaded_events.event_types,
      "event types",
    )
    return loaded_events, selected_event_types

  if requested_event_types:
    fail(
      "Event types were requested via --event-types, but no events file was found. "
      "Use --events-file or place events.out beside the main output file."
    )

  return None, []


def main(argv: Sequence[str] | None = None) -> int:
  parser = build_arg_parser()
  args = parser.parse_args(argv)

  input_path = Path(args.input_file).expanduser()
  loaded_output = load_output_table(input_path)

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
  )
  window.show()
  return application.exec()


if __name__ == "__main__":
  try:
    raise SystemExit(main())
  except ValueError as exc:
    print(f"Error: {exc}", file=sys.stderr)
    raise SystemExit(2)

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
  EVENT_DAY_END_COLUMN,
  EVENT_DAY_START_COLUMN,
  EVENT_TIMESTAMP_COLUMN,
  INTERNAL_TIMESTAMP_COLUMN,
  PLOT_COLOR_CYCLE,
  TIME_COLUMN_NAMES,
  AddColumnDialog,
  LoadedEventsData,
  LoadedSipnetData,
  TimeBounds,
  add_derived_column,
  build_timestamp,
  default_events_path,
  fail,
  find_output_header_row,
  format_time_value,
  load_events_table,
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
      loaded_events: LoadedEventsData | None,
      initial_columns: Sequence[str],
      initial_event_types: Sequence[str],
      initial_bounds: TimeBounds | None,
      initial_layout: str,
      many_columns_threshold: int,
  ) -> None:
    super().__init__()
    self.loaded = loaded
    self.loaded_events = loaded_events
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

    self.add_column_button = QPushButton("Add new column")
    self.apply_button = QPushButton("Apply")
    self.apply_button.setDefault(True)

    self.status_label = QLabel("Choose one or more columns, then click Apply.")
    self.status_label.setWordWrap(True)

    control_layout.addWidget(QLabel("Debug log prefix"))
    control_layout.addLayout(prefix_row)
    control_layout.addWidget(self.prefix_info_label)

    control_layout.addWidget(QLabel("Events output"))
    control_layout.addLayout(events_row)
    control_layout.addWidget(self.events_info_label)

    control_layout.addLayout(form_layout)
    columns_header = QHBoxLayout()
    columns_header.addWidget(QLabel("Y-axis columns"))
    columns_header.addStretch(1)
    columns_header.addWidget(self.add_column_button)
    control_layout.addLayout(columns_header)
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

    self.prefix_browse_button.clicked.connect(self.browse_for_prefix)
    self.prefix_load_button.clicked.connect(self.load_selected_debug_files)
    self.events_browse_button.clicked.connect(self.browse_for_events_file)
    self.events_load_button.clicked.connect(self.load_selected_events_file)
    self.add_column_button.clicked.connect(self.show_add_column_dialog)
    self.apply_button.clicked.connect(self.apply_view)

    self.populate_output_controls(initial_columns, initial_bounds)
    self.populate_event_controls(initial_event_types)

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
          f"Debug logs loaded, but default events file could not be loaded: {exc}",
          is_error=True,
        )
        return

      self.populate_event_controls(selected_event_types=[])
      message = (
        "Debug log files loaded. Default events file loaded. "
        "Select columns and optional event types, then click Apply."
      )
    else:
      self.loaded_events = None
      self.populate_event_controls(selected_event_types=[])
      message = (
        "Debug log files loaded. No sibling events.out was found. "
        "Select columns and click Apply, or load an events file."
      )

    self.figure.clear()
    self.canvas.draw_idle()
    self.set_status(message, is_error=False)

  def selected_plot_columns(self) -> list[str]:
    return [item.text() for item in self.columns_list.selectedItems()]

  def populate_event_controls(self, selected_event_types: Sequence[str]) -> None:
    self.event_types_list.clear()

    if self.loaded_events is None:
      requested_path = (
        Path(self.events_edit.text().strip())
        if self.events_edit.text().strip()
        else default_events_path(self.loaded.path)
      )
      self.events_info_label.setText(
        f"No events file loaded.\n"
        f"Default path: {requested_path}"
      )
      self.adjust_event_types_list_height()
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
    self.adjust_event_types_list_height()

  def adjust_event_types_list_height(self) -> None:
    if self.event_types_list.count() > 0:
      row_height = max(1, self.event_types_list.sizeHintForRow(0))
      base_height = self.event_types_list.sizeHint().height()
    else:
      row_height = max(1, self.fontMetrics().height() + 4)
      base_height = 2 * self.event_types_list.frameWidth() + 8 * row_height

    minimum_height = 2 * self.event_types_list.frameWidth() + row_height
    reduced_height = max(
      minimum_height,
      base_height - 5 * row_height,
    )
    self.event_types_list.setMaximumHeight(reduced_height)

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

  def selected_event_types(self) -> list[str]:
    return [item.text() for item in self.event_types_list.selectedItems()]

  def filtered_events(self, bounds: TimeBounds, event_types: Sequence[str]) -> pd.DataFrame | None:
    if self.loaded_events is None or not event_types:
      return None

    filtered = self.loaded_events.frame.loc[
      self.loaded_events.frame["type"].isin(event_types)
      & (self.loaded_events.frame[EVENT_DAY_START_COLUMN] <= bounds.end)
      & (self.loaded_events.frame[EVENT_DAY_END_COLUMN] >= bounds.start)
      ]
    return filtered.reset_index(drop=True)

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

    shown_event_types = set(events["type"])
    shown_types = [
      event_type
      for event_type in self.loaded_events.event_types
      if event_type in shown_event_types
    ]
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
    dialog = AddColumnDialog(
      self.create_new_column,
      self,
      expression_label=(
        "Expression (use backtick syntax for prefixed names, "
        "e.g. `envi.plantWoodC` + `flux.photosynthesis`)"
      ),
    )
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
    "-e",
    "--events-file",
    help=(
      "Optional path to an events output file (events.out). "
      "Defaults to events.out in the same directory as the debug log prefix."
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
    "--event-types",
    help="Comma-separated list of event types to pre-select in the GUI.",
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


def load_initial_events(
    args: argparse.Namespace,
    loaded: LoadedSipnetData,
) -> tuple[LoadedEventsData | None, list[str]]:
  requested_event_types = split_csv_argument(args.event_types)

  if args.events_file:
    events_path = Path(args.events_file).expanduser()
    loaded_events = load_events_table(events_path, loaded)
    selected_event_types = validate_requested_values(
      requested_event_types,
      loaded_events.event_types,
      "event types",
    )
    return loaded_events, selected_event_types

  default_path = default_events_path(loaded.path)
  if default_path.exists():
    loaded_events = load_events_table(default_path, loaded)
    selected_event_types = validate_requested_values(
      requested_event_types,
      loaded_events.event_types,
      "event types",
    )
    return loaded_events, selected_event_types

  if requested_event_types:
    fail(
      "Event types were requested via --event-types, but no events file was found. "
      "Use --events-file or place events.out beside the debug log files."
    )

  return None, []


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
  loaded_events, selected_event_types = load_initial_events(args, loaded)

  app_argv = sys.argv if argv is None else [sys.argv[0], *argv]
  application = QApplication(app_argv)
  window = SipnetDebugViewerWindow(
    loaded=loaded,
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

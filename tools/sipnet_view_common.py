import argparse
from dataclasses import dataclass
from datetime import datetime, timedelta
import keyword
import re
from pathlib import Path
from typing import Callable, Sequence, NoReturn

from matplotlib import colormaps
from matplotlib.backends.backend_qtagg import FigureCanvasQTAgg, NavigationToolbar2QT
import matplotlib.dates as mdates
from matplotlib.figure import Figure
from matplotlib.lines import Line2D
import pandas as pd
from PySide6.QtWidgets import (
  QAbstractItemView,
  QComboBox,
  QDialog,
  QDialogButtonBox,
  QFileDialog,
  QFormLayout,
  QGroupBox,
  QHBoxLayout,
  QLabel,
  QLineEdit,
  QListWidget,
  QMainWindow,
  QPushButton,
  QRadioButton,
  QVBoxLayout,
  QWidget,
)
import numpy as np

TIME_COLUMN_NAMES = ("year", "day", "time")
EVENT_COLUMN_NAMES = ("year", "day", "type")

EVENT_TIMESTAMP_COLUMN = "event_display_timestamp__"
EVENT_DAY_START_COLUMN = "event_day_start__"
EVENT_DAY_END_COLUMN = "event_day_end__"
INTERNAL_TIMESTAMP_COLUMN = "sipnet_timestamp__"
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

GROUP_BOX_STYLE = """
    QGroupBox {
        border: 2px solid gray;
        border-radius: 5px;
        margin-top: 10px; /* Leave room for the title */
        font-weight: bold;
    }
    QGroupBox::title {
        subcontrol-position: top left; /* Place on top left */
        subcontrol-origin: margin;
        padding: 2px 2px;
        color: #333333;
    }
"""

TIME_POINT_PATTERN = re.compile(
  r"^\s*(?P<year>\d{4})-(?P<day>\d{1,3})-(?P<hour>\d+(?:\.\d+)?)\s*$"
)
VARIABLE_NAME_PATTERN = re.compile(r"^[A-Za-z_][A-Za-z0-9_]*$")

# File local
def max_day_of_year(year: int) -> int:
  return (datetime(year + 1, 1, 1) - datetime(year, 1, 1)).days


def is_numeric_token(token: str) -> bool:
  try:
    float(token)
    return True
  except ValueError:
    return False


def looks_like_event_data_line(tokens: list[str]) -> bool:
  return (
      len(tokens) >= 3
      and is_numeric_token(tokens[0])
      and is_numeric_token(tokens[1])
  )


def looks_like_output_data_line(tokens: list[str]) -> bool:
  return len(tokens) >= 3 and all(is_numeric_token(token) for token in tokens[:3])


def find_events_header_row(path: Path) -> int | None:
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


def validate_new_column_name(name: str, existing_columns: Sequence[str]) -> str:
  cleaned = name.strip()
  if not cleaned:
    fail("Column name is required.")
  if VARIABLE_NAME_PATTERN.fullmatch(cleaned) is None:
    fail(
      f"Invalid column name {cleaned!r}. "
      "Use only ASCII letters, digits, and underscores, and do not start with a digit."
    )
  if keyword.iskeyword(cleaned):
    fail(f"Invalid column name {cleaned!r}. Python keywords are not allowed.")
  if cleaned in existing_columns:
    fail(f"Column name {cleaned!r} already exists.")
  return cleaned


def row_min(*cols):
  return pd.concat(cols, axis=1).min(axis=1)

def row_max(*cols):
  return pd.concat(cols, axis=1).max(axis=1)

def evaluate_new_column_expression(frame: pd.DataFrame, expression: str) -> pd.Series:
  cleaned = expression.strip()
  if not cleaned:
    fail("Expression is required.")

  try:
    result = frame.eval(cleaned, engine="python")
  except Exception as exc:
    fail(f"Failed to evaluate expression {cleaned!r}: {exc}")

  if isinstance(result, pd.DataFrame):
    fail("Expression must evaluate to a Series or scalar, not multiple columns.")
  if not isinstance(result, pd.Series):
    result = pd.Series(result, index=frame.index)
  return result


# API
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


class AddColumnDialog(QDialog):
  def __init__(
      self,
      submit_new_column: Callable[[str, str], str],
      parent: QWidget | None = None,
      expression_desc: str = "",
  ) -> None:
    super().__init__(parent)
    self.submit_new_column = submit_new_column
    self.column_name = ""

    self.setWindowTitle("Add new column")

    layout = QVBoxLayout(self)
    form_layout = QFormLayout()

    self.name_edit = QLineEdit()
    self.expression_edit = QLineEdit()
    form_layout.addRow("Name", self.name_edit)
    form_layout.addRow("Expression", self.expression_edit)
    form_layout.setFieldGrowthPolicy(QFormLayout.FieldGrowthPolicy.ExpandingFieldsGrow)
    layout.addLayout(form_layout)

    if expression_desc:
      self.desc_label = QLabel(expression_desc)
      self.desc_label.setWordWrap(True)
      layout.addWidget(self.desc_label)

    self.error_label = QLabel()
    self.error_label.setWordWrap(True)
    self.error_label.setStyleSheet("color: #a40000;")
    layout.addWidget(self.error_label)

    button_box = QDialogButtonBox(QDialogButtonBox.Ok | QDialogButtonBox.Cancel)
    button_box.accepted.connect(self.try_submit)
    button_box.rejected.connect(self.reject)
    button_box.setMinimumWidth(400)
    layout.addWidget(button_box)

  def try_submit(self) -> None:
    try:
      self.column_name = self.submit_new_column(
        self.name_edit.text(),
        self.expression_edit.text(),
      )
    except Exception as exc:
      self.error_label.setText(str(exc))
      return

    self.accept()


def add_derived_column(
    loaded_output: LoadedSipnetData,
    name: str,
    expression: str,
) -> str:
  validated_name = validate_new_column_name(name, loaded_output.frame.columns)
  loaded_output.frame[validated_name] = evaluate_new_column_expression(
    loaded_output.frame,
    expression,
  )
  loaded_output.plot_columns.append(validated_name)
  return validated_name


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
  if hour < 0 or hour >= 24:
    fail(f"Invalid hour value {hour!r}; expected 0 <= hour < 24.")

  return datetime(year, 1, 1) + timedelta(days=day - 1, hours=hour)


def fail(message: str) -> NoReturn:
  raise ValueError(message)


def format_time_value(year: int, day: int, hour: float) -> str:
  return f"{year:04d}-{day:03d}-{hour:05.2f}"


def default_events_path(output_path: Path) -> Path:
  return output_path.with_name("events.out")


def find_output_header_row(path: Path) -> int | None:
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


def load_initial_events(
    args: argparse.Namespace,
    loaded: LoadedSipnetData,
) -> tuple[LoadedEventsData | None, list[str]]:
  requested_event_types = split_csv_argument(args.event_types)

  if args.events_file:
    events_path = Path(args.events_file).expanduser()
    try:
      loaded_events = load_events_table(events_path, loaded)
    except ValueError:
      raise
    except Exception as exc:
      fail(f"Failed to load events file {events_path}: {exc}")
    selected_event_types = validate_requested_values(
      requested_event_types,
      loaded_events.event_types,
      "event types",
    )
    return loaded_events, selected_event_types

  default_path = default_events_path(loaded.path)
  if default_path.exists():
    try:
      loaded_events = load_events_table(default_path, loaded)
    except ValueError:
      raise
    except Exception as exc:
      fail(f"Failed to load default events file {default_path}: {exc}")
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


def format_datetime_for_display(value: datetime) -> str:
  year = value.year
  day = value.timetuple().tm_yday
  hour = (
      value.hour
      + value.minute / 60.0
      + value.second / 3600.0
      + value.microsecond / 3_600_000_000.0
  )
  return format_time_value(year, day, hour)


def get_default_arg_parser(desc: str) -> argparse.ArgumentParser:
  parser = argparse.ArgumentParser(description=desc)
  parser.add_argument(
    "-e",
    "--events-file",
    help=(
      "Optional path to an events output file (events.out). "
      "Defaults to events.out in the same directory as the selected input."
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
      "Use the names as shown in the Y-axis selector."
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

def find_closest_sorted(arr, target):
  # Find insertion index
  idx = np.searchsorted(arr, target)

  # Handle boundary edges
  if idx == 0:
    return 0
  if idx == len(arr):
    return -1

  # Compare neighbors to see which is closer
  before = arr[idx - 1]
  after = arr[idx]
  if after - target < target - before:
    return idx
  else:
    return idx - 1

class CustomNav2QT(NavigationToolbar2QT):
  def mouse_move(self, event):
    # Fall back to the default toolbar handler if we can't resolve a data point.
    if not event.inaxes or event.xdata is None:
      super().mouse_move(event)
      return

    # Check if the mouse is inside an Axes
    if event.inaxes:
      ax = event.inaxes
      lines = ax.get_lines()
      if not lines:
        super().mouse_move(event)
        return
      line = lines[0]
      xd = line.get_xdata()
      yd = line.get_ydata()
      target_dt = mdates.num2date(event.xdata).replace(tzinfo=None)
      target = np.datetime64(target_dt)
      near_idx = find_closest_sorted(xd, target)
      x = pd.to_datetime(xd[near_idx]).strftime('%Y-%j %H:%M')
      y = yd[near_idx]
      # Set a custom message on the toolbar status bar
      self.set_message(f"Closest data point:\nX: {x}  Y: {y}")
    else:
      # Fallback for when the mouse leaves the plot area
      self.set_message("Outside plotting area")

class SipnetViewerWindowCore(QMainWindow):
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
      browse_output: tuple[str, str],
      output_label: str,
      loaded_label: str,
      loading_label_prefix: str,
  ) -> None:
    super().__init__()
    self.loaded = loaded
    self.loaded_events = loaded_events
    self.many_columns_threshold = many_columns_threshold
    self.loaded_label = loaded_label
    self.loading_label_prefix=loading_label_prefix
    self.show_lines = True
    self.show_markers = False
    self.marker_size = 5
    self.marker_type = '.'

    self.setWindowTitle(title)
    self.resize(1500, 900)

    central_widget = QWidget(self)
    self.setCentralWidget(central_widget)
    outer_layout = QHBoxLayout(central_widget)

    control_widget = QWidget(self)
    control_layout = QVBoxLayout(control_widget)
    control_layout.setContentsMargins(0, 0, 0, 0)

    # Output
    output_row = QHBoxLayout()
    self.output_edit = QLineEdit(str(self.loaded.path))
    self.output_browse_button = QPushButton(browse_output[0])
    self.output_load_button = QPushButton(browse_output[1])
    output_row.addWidget(self.output_edit)
    output_row.addWidget(self.output_browse_button)
    output_row.addWidget(self.output_load_button)

    self.output_info_label = QLabel()
    self.output_info_label.setWordWrap(True)

    # Events
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

    # Plot Controls
    plot_row = QHBoxLayout()
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
    plot_row.addLayout(form_layout, stretch=1)

    line_display = QVBoxLayout()
    line_display.addWidget(QLabel("Lines/Markers"))
    self.lines_button = QRadioButton("Lines")
    self.lines_button.setChecked(True)
    self.lines_button.toggled.connect(lambda:self.set_line_type(self.lines_button))
    line_display.addWidget(self.lines_button)
    self.markers_button = QRadioButton("Markers")
    self.markers_button.toggled.connect(lambda:self.set_line_type(self.markers_button))
    line_display.addWidget(self.markers_button)
    self.both_button = QRadioButton("Both")
    self.both_button.toggled.connect(lambda:self.set_line_type(self.both_button))
    line_display.addWidget(self.both_button)
    plot_row.addLayout(line_display, stretch=0)

    self.columns_list = QListWidget()
    self.columns_list.setSelectionMode(QAbstractItemView.MultiSelection)

    self.event_types_list = QListWidget()
    self.event_types_list.setSelectionMode(QAbstractItemView.MultiSelection)

    self.add_column_button = QPushButton("Add new column")
    self.apply_button = QPushButton("Apply")
    self.apply_button.setDefault(True)

    self.status_label = QLabel("Choose one or more columns, then click Apply.")
    self.status_label.setWordWrap(True)

    self.output_group_box = QGroupBox(output_label)
    self.output_group_box.setStyleSheet(GROUP_BOX_STYLE)
    layout = QVBoxLayout()
    layout.addLayout(output_row)
    layout.addWidget(self.output_info_label)
    self.output_group_box.setLayout(layout)
    control_layout.addWidget(self.output_group_box)

    self.events_group_box = QGroupBox("Events output")
    self.events_group_box.setStyleSheet(GROUP_BOX_STYLE)
    layout = QVBoxLayout()
    layout.addLayout(events_row)
    layout.addWidget(self.events_info_label)
    self.events_group_box.setLayout(layout)
    control_layout.addWidget(self.events_group_box)

    self.plot_group_box = QGroupBox("Plot controls")
    self.plot_group_box.setStyleSheet(GROUP_BOX_STYLE)
    layout = QVBoxLayout()
    layout.addLayout(plot_row)
    columns_header = QHBoxLayout()
    columns_header.addWidget(QLabel("Y-axis columns"))
    columns_header.addStretch(1)
    columns_header.addWidget(self.add_column_button)
    layout.addLayout(columns_header)
    layout.addWidget(self.columns_list, stretch=1)
    layout.addWidget(QLabel("Event types"))
    layout.addWidget(self.event_types_list, stretch=0)
    layout.addWidget(self.apply_button)
    self.plot_group_box.setLayout(layout)

    control_layout.addWidget(self.plot_group_box)
    control_layout.addWidget(self.status_label)

    self.figure = Figure()
    self.canvas = FigureCanvasQTAgg(self.figure)
    self.toolbar = CustomNav2QT(self.canvas, self)

    plot_widget = QWidget(self)
    plot_layout = QVBoxLayout(plot_widget)
    plot_layout.setContentsMargins(0, 0, 0, 0)
    plot_layout.addWidget(self.toolbar)
    plot_layout.addWidget(self.canvas)

    outer_layout.addWidget(control_widget, stretch=0)
    outer_layout.addWidget(plot_widget, stretch=1)

    self.output_browse_button.clicked.connect(self.browse_for_output)
    self.output_load_button.clicked.connect(self.load_selected_output)
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

  def set_line_type(self, radio_button: QRadioButton):
    # The True->False comes in first when a new button is selected, which should
    # leave both show_lines and show_markers off
    if radio_button.text() == "Both":
      self.show_lines = radio_button.isChecked()
      self.show_markers = radio_button.isChecked()
    if radio_button.text() == "Lines":
      self.show_lines = radio_button.isChecked()
    if radio_button.text() == "Markers":
      self.show_markers = radio_button.isChecked()

  def populate_output_controls(
      self,
      selected_columns: Sequence[str],
      time_bounds: TimeBounds | None
  ) -> None:
    row_count = len(self.loaded.frame.index)
    column_count = len(self.loaded.plot_columns)
    self.output_info_label.setText(
      f"{self.loaded_label}: {self.loaded.path}\n"
      f"Rows: {row_count}\n"
      f"Plottable columns: {column_count}\n"
      f"Default time range: {self.loaded.full_start_label} to {self.loaded.full_end_label}"
    )

    if time_bounds is None:
      self.start_edit.setText(self.loaded.full_start_label)
      self.end_edit.setText(self.loaded.full_end_label)
    else:
      self.start_edit.setText(format_datetime_for_display(time_bounds.start))
      self.end_edit.setText(format_datetime_for_display(time_bounds.end))

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

  def browse_for_output(self) -> None:
    raise NotImplementedError

  @staticmethod
  def load_output(path: Path) -> LoadedSipnetData:
    raise NotImplementedError

  def load_selected_output(self) -> None:
    requested_path = Path(self.output_edit.text().strip()).expanduser()
    try:
      loaded = self.load_output(requested_path)
    except Exception as exc:
      self.set_status(str(exc), is_error=True)
      return

    self.loaded = loaded
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
          f"{self.loading_label_prefix} loaded, but default events file "
          f"could not be loaded: {exc}",
          is_error=True,
        )
        return

      self.populate_event_controls(selected_event_types=[])
      message = (
        f"{self.loading_label_prefix} loaded. Default events file loaded. "
        f"Select columns and optional event types, then click Apply."
      )
    else:
      self.loaded_events = None
      self.populate_event_controls(selected_event_types=[])
      message = (
        f"{self.loading_label_prefix} loaded. No sibling events.out was found. "
        f"Select columns and click Apply, or load an events file."
      )

    self.figure.clear()
    self.canvas.draw_idle()
    self.set_status(message, is_error=False)

  def create_new_column(self, name: str, expression: str) -> str:
    new_name = add_derived_column(self.loaded, name, expression)
    self.columns_list.addItem(new_name)
    item = self.columns_list.item(self.columns_list.count() - 1)
    item.setSelected(True)
    return new_name

  def show_add_column_dialog(self) -> None:
    raise NotImplementedError

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

    shown_event_types = set(events["type"])
    shown_types = [event_type for event_type in self.loaded_events.event_types if event_type in shown_event_types]
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

    line_type_args = {}
    if self.show_markers:
      line_type_args['marker'] = self.marker_type
      line_type_args['markersize'] = self.marker_size
      line_type_args['linestyle'] = 'None'
    if self.show_lines:
      line_type_args['linestyle'] = '-'  # override, if both are set
      line_type_args['linewidth'] = 1.5

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
        zorder=2,
        **line_type_args,
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
    line_type_args = {}
    if self.show_markers:
      line_type_args['marker'] = self.marker_type
      line_type_args['markersize'] = self.marker_size
      line_type_args['linestyle'] = 'None'
    if self.show_lines:
      line_type_args['linestyle'] = '-'  # override, if both are set
      line_type_args['linewidth'] = 1.5

    x_values = frame[INTERNAL_TIMESTAMP_COLUMN]
    axes = self.figure.subplots(len(columns), 1, sharex=True, squeeze=False)
    flat_axes = [axis for row in axes for axis in row]

    event_handles: list[Line2D] = []
    for index, (axis, column) in enumerate(zip(flat_axes, columns)):
      color = PLOT_COLOR_CYCLE[index % len(PLOT_COLOR_CYCLE)]
      axis.plot(x_values, frame[column], color=color, zorder=2, **line_type_args)
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

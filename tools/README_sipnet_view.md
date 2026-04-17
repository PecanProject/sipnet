# SIPNET Output Viewer

Interactive viewer for `sipnet.out` files, with optional overlays from `events.out`.

## Features

- Loads `sipnet.out` by default, or a file passed on the command line
- Dynamically reads output headers from the file
- Requires a main-output header row beginning with `year day time`
- Uses `year`, `day`, and `time` to build the x-axis
- Excludes `year`, `day`, and `time` from y-axis selections
- Loads `events.out` from the same directory by default when present
- Lets you browse and load a different events file
- Reads event rows from the first three columns: `year`, `day`, `type`
- Ignores the fourth event column
- Lets you choose which event types to display
- Draws dotted vertical event lines color-coded by event type
- Supports:
  - combined view with twinned y-axes
  - split subplot view
- Does not redraw on every selection change; redraw happens only when `Apply` is clicked
- Defaults the GUI time range to the full file
- Can pre-populate selected columns, event types, and time range from CLI options

## Install

```zsh
python3 -m venv .venv
source .venv/bin/activate
python3 -m pip install -r requirements.txt

## Run Examples

From the repository root:
```zsh
python3 tools/sipnet_view.py
```
Open a specific file:
```zsh
python3 tools/sipnet_view.py --input-file tests/smoke/russell_1/sipnet.out
```

Open a specific output file and a custom events file:
```zsh
python3 tools/sipnet_view.py \
  --input-file tests/smoke/russell_1/sipnet.out \
  --events-file tests/smoke/russell_1/events.out
```

Pre-populate columns:
```zsh
python3 tools/sipnet_view.py --input-file tests/smoke/russell_1/sipnet.out --columns "gpp, npp, ra"
```

Pre-populate event types:
```zsh
pyhon3 tools/sipnet_view.py \
--input-file tests/smoke/russell_1/sipnet.out \
--event-types leafon,irrig
````

Pre-populate time range:
```zsh
python3 tools/sipnet_view.py \
  --input-file tests/smoke/russell_1/sipnet.out \
  --time-range 2016-001-00.00,2016-010-12.00
```

Use subplot mode:
```zsh
python3 tools/sipnet_view.py \
  --input-file tests/smoke/russell_1/sipnet.out \
  --columns plantWoodC,soil,nee \
  --layout subplots
```

CLI options
* --input-file: path to the SIPNET output file
* --events-file: path to the events file; defaults to sibling events.out
* --time-range: initial range as YYYY-DOY-HH,YYYY-DOY-HH
* --columns: comma-separated list of output columns to pre-select
* --event-types: comma-separated list of event types to pre-select
* --layout: combined or subplots
* --many-columns-threshold: accepted for compatibility, currently inactive

Notes
* If the main output file has no header row, the tool exits with an error.
* Leading note lines before the main output header are allowed.
* If the events file is missing, the viewer still works for sipnet.out.
* Event-type selections default to off.
* Empty Apply shows an inline status message and leaves the current plot unchanged.
* Invalid --time-range, unknown --columns, or unknown --event-types fail fast before the GUI opens.

Quick Test:
```zsh
python3 tools/test_sipnet_view.py
```

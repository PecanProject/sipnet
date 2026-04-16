# SIPNET Output Viewer

Interactive viewer for `sipnet.out` files using PySide6 and matplotlib.

## Features

- Loads `sipnet.out` by default, or a file passed on the command line
- Dynamically reads headers from the file
- Requires a header row beginning with `year day time`
- Uses `year`, `day`, and `time` to build the x-axis
- Excludes `year`, `day`, and `time` from y-axis selections
- Supports:
    - combined view with twinned y-axes
    - split subplot view
- Does not redraw on every selection change; redraw happens only when `Apply` is clicked
- Defaults the GUI time range to the full file
- Can pre-populate selected columns and time range from CLI options

## Install

```zsh
python3 -m venv .venv
source .venv/bin/activate
python3 -m pip install -r requirements.txt
```

## Run Examples

From the repository root:
```zsh
python3 tools/sipnet_view.py
```
Open a specific file:
```zsh
python3 tools/sipnet_view.py --input-file tests/smoke/russell_1/sipnet.out
```

Pre-populate columns:
```zsh
python3 tools/sipnet_view.py --input-file tests/smoke/russell_1/sipnet.out --columns "gpp, npp, ra"
```

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
--input-file: path to the SIPNET output file
--time-range: initial range as YYYY-DOY-HH,YYYY-DOY-HH
--columns: comma-separated list of columns to pre-select
--layout: combined or subplots
--many-columns-threshold: accepted for compatibility, currently inactive

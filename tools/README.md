## SIPNET Tools

This directory contains tools related to SIPNET, including:
- `sipnet-view`: an interactive viewer for `sipnet.out` files, with optional overlays from `events.out`.
- `smoke-check`: a script to compare generated `sipnet.out` and `events.out` files to expected outputs for smoke tests.

## Install

```zsh
python3 -m venv .venv
source .venv/bin/activate
python3 -m pip install .
# or, if developing and want changes to be reflected without reinstalling:
python3 -m pip install -e .
```

## More Info

- See the [SIPNET Viewer README](README_sipnet_view.md) for details on the viewer tool.
- See the [smoke check README](README_smoke_check.md) for details on the smoke-check tool.

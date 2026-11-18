"""Train, calibrate, evaluate and export SIPNET Downscaling models."""

import argparse
import json
from downscaling.train.configuration import load


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("command", choices=("preflight", "run", "report"))
    parser.add_argument("--config", required=True)
    parser.add_argument("--resume", action="store_true")
    args = parser.parse_args()
    if args.resume and args.command != "run":
        parser.error("--resume requires run")
    try:
        from downscaling.train.engine import execute

        result = execute(args.command, load(args.config), args.resume)
    except (ValueError, RuntimeError, FileNotFoundError) as error:
        print(json.dumps(dict(status="incomplete", error=str(error))), flush=True)
        raise SystemExit(1) from error
    print(json.dumps(result), flush=True)


if __name__ == "__main__":
    main()

"""Command-line entry point for bounded flux inference."""

import argparse
import json
from downscaling.configuration import load


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("command", choices=["preflight", "run", "report"])
    parser.add_argument("--config", required=True)
    parser.add_argument("--resume", action="store_true")
    args = parser.parse_args()
    if args.resume and args.command != "run":
        parser.error("--resume is only valid with run")
    config = load(args.config)
    from downscaling.predict.engine import execute

    try:
        result = execute(args.command, config, resume=args.resume)
    except (ValueError, RuntimeError, FileNotFoundError) as error:
        print(json.dumps(dict(status="incomplete", error=str(error))))
        raise SystemExit(1) from error
    print(json.dumps(result))


if __name__ == "__main__":
    main()

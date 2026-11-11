"""Train, calibrate, evaluate and export SIPNET Downscaling models."""

import argparse
import json
from downscaling.train.configuration import load


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "command", choices=["preflight", "run", "report", "compare-history"]
    )
    parser.add_argument("--config", required=True)
    parser.add_argument("--resume", action="store_true")
    parser.add_argument(
        "--reference", help="Historical reference directory (compare-history only)"
    )
    parser.add_argument(
        "--comparison-output", help="Separate comparison output directory"
    )
    args = parser.parse_args()
    if args.resume and args.command != "run":
        parser.error("--resume is only valid with run")
    if args.command == "compare-history":
        if not args.reference or not args.comparison_output:
            parser.error("compare-history requires --reference and --comparison-output")
    elif args.reference or args.comparison_output:
        parser.error("Historical arguments are only valid with compare-history")
    try:
        config = load(args.config)
        from downscaling.train.engine import execute

        if args.command == "compare-history":
            if execute("report", config)["status"] != "complete":
                raise ValueError(
                    "Complete the training run before historical comparison"
                )
            from downscaling.train.state import Run
            from downscaling.train.history import compare

            result = compare(Run(config), args.reference, args.comparison_output)
        else:
            result = execute(args.command, config, args.resume)
    except (ValueError, RuntimeError, FileNotFoundError) as error:
        print(json.dumps(dict(status="incomplete", error=str(error))), flush=True)
        raise SystemExit(1) from error
    print(json.dumps(result), flush=True)


if __name__ == "__main__":
    main()

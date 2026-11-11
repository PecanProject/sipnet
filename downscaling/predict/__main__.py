"""Command-line entry point for bounded flux inference."""
import argparse
import json
from downscaling.configuration import load


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('command',choices=['preflight','run','report'])
    parser.add_argument('--config',required=True)
    parser.add_argument('--resume',action='store_true')
    args = parser.parse_args()
    if args.resume and args.command != 'run':
        parser.error('--resume is only valid with run')
    config = load(args.config)
    if args.command == 'preflight':
        print(json.dumps({'configuration':'valid','output':str(config.output)}))
    else:
        from downscaling.predict.engine import execute
        execute(config,args.command,resume=args.resume)

if __name__ == '__main__':
    main()

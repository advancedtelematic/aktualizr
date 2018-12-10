#!/usr/bin/env python3

import argparse
import sys
import pprint
import shlex
import yaml

from pathlib import Path


def gen_test_script(ty, job, output):
    output.write('#!/usr/bin/env bash\n\n')
    output.write('set -ex\n')

    # extract environment variables
    e_str = ty['env'][job]
    for v_assign in shlex.split(e_str):
        output.write(v_assign + '\n')
    output.write('\n')

    # extract script lines
    for l in ty['script']:
        output.write(l + '\n')


def main():
    parser = argparse.ArgumentParser(description='Run travis jobs locally')
    parser.add_argument('--yml', '-y', metavar='travis.yml', type=Path,
                        default=Path('.travis.yml'), help='.travis.yml file')
    parser.add_argument('--job', '-j', metavar='JOB', type=int, default=0)
    parser.add_argument('--output', '-o', metavar='OUTPUT.sh', type=argparse.FileType('w'),
                        default=sys.stdout)
    parser.add_argument('--verbose', '-v', action='store_true')
    args = parser.parse_args()

    ymlf = args.yml
    with open(ymlf, 'r') as f:
        yml = f.read()

    ty = yaml.load(yml)

    if args.verbose:
        pp = pprint.PrettyPrinter(indent=4, stream=sys.stderr)
        pp.pprint(ty)

    gen_test_script(ty, args.job, args.output)

    return 0


if __name__ == '__main__':
    sys.exit(main())

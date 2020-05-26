#!/usr/bin/env python3

import argparse
import contextlib
import functools
import multiprocessing
import os
import shutil
import subprocess
import sys
import tempfile
import time

from os import path
from subprocess import run

from fake_http_server.fake_test_server import FakeTestServerBackground


"""
Run aktualizr provisioning and update cycles with random IO failures,
then retry the procedure and make sure the second attempt succeeds.

Right now, the executable should be aktualizr-cycle-simple which has
been fine-tuned for this test, but others can be adapted on the same
model.

On a sample run, this test calls ~3000/4000 io syscalls, so it's recommended
to run with pf = 1/4000 = 0.00025, so that there should be around one error
per run, for around 4000*log(4000) ~= 50000 runs
(see https://en.wikipedia.org/wiki/Coupon_collector%27s_problem)
"""


@contextlib.contextmanager
def uptane_repo(uptane_gen):
    with tempfile.TemporaryDirectory() as repo_path:
        arepo = [uptane_gen, '--keytype', 'ed25519']
        run([*arepo, 'generate', '--path', repo_path])
        fw_path = path.join(repo_path, 'images/firmware.txt')
        os.makedirs(path.join(repo_path, 'images'))
        with open(fw_path, 'wb') as f:
            f.write(b'fw')
        run([*arepo, 'image', '--path', repo_path, '--filename', fw_path,
             '--targetname', 'firmware.txt'])
        run([*arepo, 'addtarget', '--path', repo_path, '--targetname',
             'firmware.txt', '--hwid', 'primary_hw', '--serial', 'CA:FE:A6:D2:84:9D'])
        run([*arepo, 'signtargets', '--path', repo_path])
        yield repo_path


def run_test(akt_test, server, srcdir, pf, k):
    print(f'Running test {k}')
    with tempfile.TemporaryDirectory() as storage_dir:
        # run once with (maybe) a fault, trying to update
        cp_err = run(['fiu-run', '-x', '-c', f'enable_random name=posix/io/*,probability={pf}',
                      akt_test, storage_dir, server],
                     stdout=subprocess.PIPE, stderr=subprocess.STDOUT, cwd=srcdir)
        if cp_err.returncode == 0:
            return True

        print('update failed, checking state')
        cp = run([akt_test, storage_dir, server],
                 stdout=subprocess.PIPE, stderr=subprocess.STDOUT, cwd=srcdir)
        if cp.returncode != 0:
            error_state_dir = f'fail_state.{path.basename(storage_dir)}'
            print(f'Error detected, see {error_state_dir}')
            shutil.copytree(storage_dir, error_state_dir)
            with open(path.join(error_state_dir, 'output1.log'), 'wb') as f:
                f.write(cp_err.stdout)
            with open(path.join(error_state_dir, 'output2.log'), 'wb') as f:
                f.write(cp.stdout)
            return False
        return True


def main():
    parser = argparse.ArgumentParser(description='Run io failure tests')
    parser.add_argument('-n', '--n-tests', type=int, default=100,
                        help='number of random tests to run')
    parser.add_argument('-j', '--jobs', type=int, default=1,
                        help='number of parallel tests')
    parser.add_argument('-pf', '--probability-failure', type=float, default=0.00025,
                        help='probability of syscall failure')
    parser.add_argument('--akt-srcdir', help='path to the aktualizr source directory')
    parser.add_argument('--uptane-gen', help='path to uptane-generator executable')
    parser.add_argument('--akt-test', help='path to aktualizr cycle test')
    parser.add_argument('--serve-only', action='store_true',
                        help='only serve metadata, do not run tests')
    args = parser.parse_args()

    srcdir = path.abspath(args.akt_srcdir) if args.akt_srcdir is not None else os.getcwd()

    with uptane_repo(args.uptane_gen) as repo_dir, \
            FakeTestServerBackground(repo_dir, srcdir=srcdir) as uptane_server, \
            multiprocessing.Pool(args.jobs) as pool:

        server = f'http://localhost:{uptane_server.port}'
        print(f'Running tests on {server} (repo directory: {repo_dir})')

        if args.serve_only:
            while True:
                time.sleep(1)
            return 0

        fk = functools.partial(run_test, path.abspath(args.akt_test),
                              server, srcdir, args.probability_failure)
        for r in pool.imap(fk, range(1, args.n_tests+1)):
            if not r:
                print('error found!')
                pool.close()
                break

    return 0


if __name__ == '__main__':
    sys.exit(main())

#!/usr/bin/env python3

import argparse
import logging
import signal
import time

from os import getcwd, chdir

from test_fixtures import with_aktualizr, with_uptane_backend, KeyStore, with_director


logger = logging.getLogger(__file__)


@with_uptane_backend(start_generic_server=True)
@with_director(start=True)
@with_aktualizr(start=False, log_level=0, run_mode='full')
def test_aktualizr_kill(director, aktualizr, **kwargs):
    test_result = False
    with aktualizr:
        try:
            aktualizr.wait_for_provision()
            aktualizr.terminate()
            aktualizr.wait_for_completion()
            test_result = 'Aktualizr daemon exiting...' in aktualizr.output()
        except Exception:
            aktualizr.terminate(sig=signal.SIGKILL)
            aktualizr.wait_for_completion()
            print(aktualizr.output())
            raise

    return test_result


if __name__ == "__main__":
    logging.basicConfig(level=logging.INFO)

    initial_cwd = getcwd()
    parser = argparse.ArgumentParser(description='Test aktualizr kill')
    parser.add_argument('-b', '--build-dir', help='build directory', default=initial_cwd + '/build')
    parser.add_argument('-s', '--src-dir', help='source directory', default=initial_cwd)
    input_params = parser.parse_args()

    KeyStore.base_dir = input_params.src_dir
    chdir(input_params.build_dir)

    test_suite = [
        test_aktualizr_kill,
    ]

    test_suite_run_result = True
    for test in test_suite:
        logger.info('>>> Running {}...'.format(test.__name__))
        test_run_result = test()
        logger.info('>>> {}: {}\n'.format('OK' if test_run_result else 'FAILED', test.__name__))
        test_suite_run_result = test_suite_run_result and test_run_result

    chdir(initial_cwd)
    exit(0 if test_suite_run_result else 1)

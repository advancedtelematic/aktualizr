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

    parser = argparse.ArgumentParser(description='Test aktualizr kill')
    parser.add_argument('-b', '--build-dir', help='build directory', default='build')
    parser.add_argument('-s', '--src-dir', help='source directory', default='.')
    input_params = parser.parse_args()

    KeyStore.base_dir = input_params.src_dir
    initial_cwd = getcwd()
    chdir(input_params.build_dir)

    test_suite = [
        test_aktualizr_kill,
    ]

    test_suite_run_result = True
    for test in test_suite:
        logger.info('>>> Running {}...'.format(test.__name__))
        test_run_result = test()
        logger.info('>>> {}: {}'.format('OK' if test_run_result else 'Failed', test.__name__))
        test_suite_run_result = test_suite_run_result and test_run_result

    chdir(initial_cwd)
    exit(0 if test_suite_run_result else 1)

#!/usr/bin/env python3

import logging
import argparse

from os import getcwd, chdir
from test_fixtures import KeyStore, with_uptane_backend, with_path, with_director, with_aktualizr,\
    with_install_manager, with_imagerepo, TestRunner, \
    DownloadInterruptionHandler, MalformedJsonHandler, DownloadInterruptionHandler

logger = logging.getLogger(__file__)


"""
Verifies whether aktualizr is updatable after director metadata download failure
with follow-up successful metadata download.

Currently, it's tested against two types of metadata download/parsing failure:
    - download interruption - metadata file download is interrupted once|three times, after that it's successful
    - malformed json - aktualizr receives malformed json/metadata as a response to the first request for metadata,
      a response to subsequent request is successful

Note: Aktualizr doesn't send any installation report in manifest in case of metadata download failure
https://saeljira.it.here.com/browse/OTA-3730
"""
@with_uptane_backend(start_generic_server=True)
@with_path(paths=['/1.root.json', '/targets.json'])
@with_director(handlers=[
                            DownloadInterruptionHandler(number_of_failures=1),
                            MalformedJsonHandler(number_of_failures=1),
                            DownloadInterruptionHandler(number_of_failures=3),
                        ], start=False)
@with_aktualizr(start=False, run_mode='full')
@with_install_manager()
def test_director_update_after_metadata_download_failure(install_mngr, director,
                                                         aktualizr, **kwargs):
    with director:
        with aktualizr:
            install_result = director.wait_for_install()
            install_result = install_result and install_mngr.are_images_installed()
    return install_result


"""
  Verifies whether an update fails if director metadata download fails or they are malformed
  - download is interrupted three times
  - malformed json is received
"""
@with_uptane_backend(start_generic_server=True)
@with_path(paths=['/1.root.json', '/targets.json'])
@with_imagerepo()
@with_director(handlers=[
                            DownloadInterruptionHandler(number_of_failures=3),
                            MalformedJsonHandler(number_of_failures=1),
                        ])
@with_aktualizr(run_mode='once')
@with_install_manager()
def test_director_unsuccessful_download(install_mngr, aktualizr,
                                        director, **kwargs):
    aktualizr.wait_for_completion()
    return not (director.get_install_result() or install_mngr.are_images_installed())


if __name__ == "__main__":
    logging.basicConfig(level=logging.INFO)

    parser = argparse.ArgumentParser(description='Test backend failure')
    parser.add_argument('-b', '--build-dir', help='build directory', default='build')
    parser.add_argument('-s', '--src-dir', help='source directory', default='.')
    input_params = parser.parse_args()

    KeyStore.base_dir = input_params.src_dir
    initial_cwd = getcwd()
    chdir(input_params.build_dir)

    test_suite = [
                    test_director_update_after_metadata_download_failure,
                    test_director_unsuccessful_download
    ]

    test_suite_run_result = TestRunner(test_suite).run()

    chdir(initial_cwd)
    exit(0 if test_suite_run_result else 1)

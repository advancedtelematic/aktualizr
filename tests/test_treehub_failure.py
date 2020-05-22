#!/usr/bin/env python3

import logging
import argparse

from os import getcwd, chdir
from test_fixtures import KeyStore, with_uptane_backend, with_path, with_director, with_aktualizr, \
    with_sysroot, with_treehub, DownloadInterruptionHandler, MalformedImageHandler, RedirectHandler, TestRunner

logger = logging.getLogger(__file__)


"""
    Verifies whether aktualizr is updatable after failure of object(s) download from Treehub/ostree repo
    with follow-up successful download.

    Currently, it's tested against two types of object download failure:
    - download interruption - object download is interrupted once, after that it's successful
    - malformed object - object download is successful but it's malformed. It happens once after that it's successful
"""
@with_uptane_backend(start_generic_server=True)
@with_director()
@with_treehub(handlers=[
                        DownloadInterruptionHandler(url='/objects/41/5ce9717fc7a5f4d743a4f911e11bd3ed83930e46756303fd13a3eb7ed35892.filez'),
                        MalformedImageHandler(url='/objects/41/5ce9717fc7a5f4d743a4f911e11bd3ed83930e46756303fd13a3eb7ed35892.filez'),
                        RedirectHandler(number_of_redirects=1000, url='/objects/41/5ce9717fc7a5f4d743a4f911e11bd3ed83930e46756303fd13a3eb7ed35892.filez'),

                        # TODO: ostree objects download is not resilient to `Slow Retrieval Attack`
                        # https://saeljira.it.here.com/browse/OTA-3737
                        #SlowRetrievalHandler(url='/objects/6b/1604b586fcbe052bbc0bd9e1c8040f62e085ca2e228f37df957ac939dff361.filez'),

])
@with_sysroot()
@with_aktualizr(start=False, run_mode='once')
def test_treehub_update_after_image_download_failure(uptane_repo,
                                                     aktualizr,
                                                     director,
                                                     uptane_server,
                                                     sysroot, treehub):
    target_rev = treehub.revision
    uptane_repo.add_ostree_target(aktualizr.id, target_rev)
    with aktualizr:
        aktualizr.wait_for_completion()

    pending_rev = aktualizr.get_primary_pending_version()
    if pending_rev != target_rev:
        logger.error("Pending version {} != the target one {}".format(pending_rev, target_rev))
        return False

    sysroot.update_revision(pending_rev)
    aktualizr.emulate_reboot()

    with aktualizr:
        aktualizr.wait_for_completion()

    result = director.get_install_result() and (target_rev == aktualizr.get_current_primary_image_info())
    return result


"""
    Verifies that aktualizr does not install an image which contains files with wrong checksums
"""
@with_uptane_backend(start_generic_server=True)
@with_director()
@with_treehub(handlers=[
    MalformedImageHandler(url='/objects/41/5ce9717fc7a5f4d743a4f911e11bd3ed83930e46756303fd13a3eb7ed35892.filez',
                          number_of_failures=-1, fake_filez=True),

])
@with_sysroot()
@with_aktualizr(start=False, run_mode='once', output_logs=True)
def test_treehub_update_if_bad_ostree_checksum(uptane_repo,
                                               aktualizr,
                                               director,
                                               uptane_server,
                                               sysroot, treehub):
    target_rev = treehub.revision
    uptane_repo.add_ostree_target(aktualizr.id, target_rev)
    with aktualizr:
        aktualizr.wait_for_completion()

    pending_rev = aktualizr.get_primary_pending_version()
    if pending_rev == target_rev:
        logger.error("Pending version {} == the target one {}".format(pending_rev, target_rev))
        return False
    return True


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
                    test_treehub_update_after_image_download_failure,
                    test_treehub_update_if_bad_ostree_checksum
    ]

    test_suite_run_result = TestRunner(test_suite).run()

    chdir(initial_cwd)
    exit(0 if test_suite_run_result else 1)

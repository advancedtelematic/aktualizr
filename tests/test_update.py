#!/usr/bin/env python3

import logging
import argparse
import time

from os import getcwd, chdir

from test_fixtures import KeyStore, with_aktualizr, with_uptane_backend, with_secondary, with_director, with_imagerepo,\
    with_sysroot, with_treehub


logger = logging.getLogger(__file__)


"""
 Test update of Primary and Secondary if their package manager differs, `ostree`
 and binary (`none` or `fake`) respectively

 Aktualizr/Primary's package manager is set to `ostree`
 Secondary's package manager is set to `fake` which means a file/binary update
 Primary goal is to verify whether aktualizr succeeds with a binary/fake update of secondary
 while aktualizr/primary is configured with ostree package manager
"""
@with_uptane_backend(start_generic_server=True)
@with_secondary(start=True)
@with_director()
@with_treehub()
@with_sysroot()
@with_aktualizr(start=False, run_mode='once', output_logs=True)
def test_primary_ostree_secondary_file_updates(uptane_repo, secondary, aktualizr, director, sysroot,
                                               treehub, uptane_server, **kwargs):
    target_rev = treehub.revision
    # add an ostree update for Primary
    uptane_repo.add_ostree_target(aktualizr.id, target_rev)
    # add a fake/binary update for Secondary
    secondary_update_hash = uptane_repo.add_image(secondary.id, "secondary-update.bin")

    with aktualizr:
        aktualizr.wait_for_completion()

    # check the Primary update, must be in pending state since it requires reboot
    pending_rev = aktualizr.get_primary_pending_version()
    if pending_rev != target_rev:
        logger.error("Pending version {} != the target version {}".format(pending_rev, target_rev))
        return False

    # check the Secondary update
    current_secondary_image_hash = aktualizr.get_current_image_info(secondary.id)
    if current_secondary_image_hash != secondary_update_hash:
        logger.error("Current secondary image {} != expected image {}".format(current_secondary_image_hash,
                                                                              secondary_update_hash))
        return False

    # emulate reboot and run aktualizr once more
    sysroot.update_revision(pending_rev)
    aktualizr.emulate_reboot()

    with aktualizr:
        aktualizr.wait_for_completion()

    # check the Primary update after reboot
    result = director.get_install_result() and (target_rev == aktualizr.get_current_primary_image_info())
    return result


"""
 Test update of Secondary's ostree repo if an ostree target metadata are expired

 Metadata are valid at the moment of a new ostree revision installation,
 but are expired after that and before Secondary is rebooted,
 we still expect that the installed update is applied in this case
"""
@with_treehub()
@with_uptane_backend()
@with_director()
@with_sysroot()
@with_secondary(start=False)
@with_aktualizr(start=False, run_mode='once', output_logs=True)
def test_secodary_ostree_update_if_metadata_expires(uptane_repo, secondary, aktualizr, treehub, sysroot, director, **kwargs):
    target_rev = treehub.revision
    expires_within_sec = 10

    uptane_repo.add_ostree_target(secondary.id, target_rev, expires_within_sec=expires_within_sec)
    start_time = time.time()

    with secondary:
        with aktualizr:
            aktualizr.wait_for_completion()

    pending_rev = aktualizr.get_current_pending_image_info(secondary.id)

    if pending_rev != target_rev:
        logger.error("Pending version {} != the target one {}".format(pending_rev, target_rev))
        return False

    # wait until the target metadata are expired
    time.sleep(max(0, expires_within_sec - (time.time() - start_time)))

    sysroot.update_revision(pending_rev)
    secondary.emulate_reboot()

    with secondary:
        with aktualizr:
            aktualizr.wait_for_completion()

    if not director.get_install_result():
        logger.error("Installation result is not successful")
        return False

    installed_rev = aktualizr.get_current_image_info(secondary.id)

    if installed_rev != target_rev:
        logger.error("Installed version {} != the target one {}".format(installed_rev, target_rev))
        return False

    return True


"""
 Test update of Primary's ostree repo if an ostree target metadata are expired

 Metadata are valid at the moment of a new ostree revision installation,
 but are expired after that and before Primary is rebooted,
 we still expect that the installed update is applied in this case
"""
@with_uptane_backend(start_generic_server=True)
@with_director()
@with_treehub()
@with_sysroot()
@with_aktualizr(start=False, run_mode='once', output_logs=True)
def test_primary_ostree_update_if_metadata_expires(uptane_repo, aktualizr, director, sysroot, treehub, uptane_server, **kwargs):
    target_rev = treehub.revision
    expires_within_sec = 10

    # add an ostree update for Primary
    uptane_repo.add_ostree_target(aktualizr.id, target_rev, expires_within_sec=expires_within_sec)
    start_time = time.time()

    with aktualizr:
        aktualizr.wait_for_completion()

    # check the Primary update, must be in pending state since it requires reboot
    pending_rev = aktualizr.get_primary_pending_version()
    if pending_rev != target_rev:
        logger.error("Pending version {} != the target version {}".format(pending_rev, target_rev))
        return False

    # wait until the target metadata are expired
    time.sleep(max(0, expires_within_sec - (time.time() - start_time)))

    # emulate reboot and run aktualizr once more
    sysroot.update_revision(pending_rev)
    aktualizr.emulate_reboot()

    with aktualizr:
        aktualizr.wait_for_completion()

    # check the Primary update after reboot
    result = director.get_install_result() and (target_rev == aktualizr.get_current_primary_image_info())
    return result


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
        test_primary_ostree_secondary_file_updates,
        test_secodary_ostree_update_if_metadata_expires,
        test_primary_ostree_update_if_metadata_expires
    ]

    test_suite_run_result = True
    for test in test_suite:
        logger.info('>>> Running {}...'.format(test.__name__))
        test_run_result = test()
        logger.info('>>> {}: {}\n'.format('OK' if test_run_result else 'FAILED', test.__name__))
        test_suite_run_result = test_suite_run_result and test_run_result

    chdir(initial_cwd)
    exit(0 if test_suite_run_result else 1)

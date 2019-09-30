#!/usr/bin/env python3

import logging
import argparse

from os import getcwd, chdir

from test_fixtures import KeyStore, with_aktualizr, with_uptane_backend, with_secondary, with_director, with_imagerepo,\
    with_sysroot, with_treehub


logger = logging.getLogger(__file__)


"""
 Test update of Primary and Secondary if their package manager differs, `ostree` and `fake` respectively
 
 Aktualizr/Primary's package manager is set to `ostree`
 Secondary's package manager is set to `fake`
 Primary goal is to verify whether aktualizr succeeds with a binary/fake update of secondary
 while aktualizr/primary is configured with ostree package manager   
"""
@with_uptane_backend(start_generic_server=True)
@with_director()
@with_treehub()
@with_sysroot()
@with_secondary(start=True)
@with_aktualizr(start=False, run_mode='once', output_logs=True)
def test_primary_ostree_secondary_fake_updates(uptane_repo, secondary, aktualizr, director,
                                               uptane_server, sysroot, treehub):
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
        logger.error("Pending version {} != the target one {}".format(pending_rev, target_rev))
        return False

    # check the Secondary update
    current_secondary_image_hash = aktualizr.get_current_image_info(secondary.id)
    if current_secondary_image_hash != secondary_update_hash:
        logger.error("Secondary current image {} != {} expected one".format(current_secondary_image_hash,
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
        test_primary_ostree_secondary_fake_updates
    ]

    test_suite_run_result = True
    for test in test_suite:
        logger.info('>>> Running {}...'.format(test.__name__))
        test_run_result = test()
        logger.info('>>> {}: {}'.format('OK' if test_run_result else 'Failed', test.__name__))
        test_suite_run_result = test_suite_run_result and test_run_result

    chdir(initial_cwd)
    exit(0 if test_suite_run_result else 1)


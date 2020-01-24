#!/usr/bin/env python3

import argparse
import logging
import threading
import time

from os import getcwd, chdir, path

from test_fixtures import with_aktualizr, with_uptane_backend, KeyStore, with_secondary, with_treehub,\
    with_sysroot, with_director

logger = logging.getLogger("IPSecondaryTest")


# The following is a test suit intended for IP Secondary integration testing
@with_uptane_backend()
@with_secondary(start=True)
@with_aktualizr(start=False, output_logs=False)
def test_secondary_update_if_secondary_starts_first(uptane_repo, secondary, aktualizr, **kwargs):
    '''Test Secondary update if Secondary is booted before Primary'''

    # add a new image to the repo in order to update the secondary with it
    secondary_image_filename = "secondary_image_filename_001.img"
    secondary_image_hash = uptane_repo.add_image(id=secondary.id, image_filename=secondary_image_filename)

    logger.debug("Trying to update ECU {} with the image {}".
                format(secondary.id, (secondary_image_hash, secondary_image_filename)))

    with aktualizr:
        # run aktualizr once, secondary has been already running
        aktualizr.wait_for_completion()

    test_result = secondary_image_hash == aktualizr.get_current_image_info(secondary.id)
    logger.debug("Update result: {}".format("success" if test_result else "failed"))
    return test_result


@with_uptane_backend()
@with_secondary(start=False)
@with_aktualizr(start=True, output_logs=False)
def test_secondary_update_if_primary_starts_first(uptane_repo, secondary, aktualizr, **kwargs):
    '''Test Secondary update if Secondary is booted after Primary'''

    # add a new image to the repo in order to update the secondary with it
    secondary_image_filename = "secondary_image_filename_001.img"
    secondary_image_hash = uptane_repo.add_image(id=secondary.id, image_filename=secondary_image_filename)

    logger.debug("Trying to update ECU {} with the image {}".
                format(secondary.id, (secondary_image_hash, secondary_image_filename)))
    with secondary:
        # start secondary, aktualizr has been already started in 'once' mode
        aktualizr.wait_for_completion()

    test_result = secondary_image_hash == aktualizr.get_current_image_info(secondary.id)
    logger.debug("Update result: {}".format("success" if test_result else "failed"))
    return test_result


@with_uptane_backend()
@with_director()
@with_secondary(start=False)
@with_aktualizr(start=False, output_logs=False)
def test_secondary_update(uptane_repo, secondary, aktualizr, director, **kwargs):
    '''Test Secondary update if a boot order of Secondary and Primary is undefined'''

    test_result = True
    # add a new image to the repo in order to update the secondary with it
    secondary_image_filename = "secondary_image_filename.img"
    secondary_image_hash = uptane_repo.add_image(id=secondary.id, image_filename=secondary_image_filename)

    logger.debug("Trying to update ECU {} with the image {}".
                format(secondary.id, (secondary_image_hash, secondary_image_filename)))

    # start Secondary and Aktualizr processes, aktualizr is started in 'once' mode
    with secondary, aktualizr:
        aktualizr.wait_for_completion()

    if not director.get_install_result():
        logger.error("Installation result is not successful")
        return False

    # check currently installed hash
    if secondary_image_hash != aktualizr.get_current_image_info(secondary.id):
        logger.error("Target image hash doesn't match the currently installed hash")
        return False

    # check updated file
    update_file = path.join(secondary.storage_dir.name, "firmware.txt")
    if not path.exists(update_file):
        logger.error("Expected updated file does not exist: {}".format(update_file))
        return False

    if secondary_image_filename != director.get_ecu_manifest_filepath(secondary.id[1]):
        logger.error("Target name doesn't match a filepath value of the reported manifest: {}".format(director.get_manifest()))
        return False

    return True


@with_treehub()
@with_uptane_backend()
@with_director()
@with_sysroot()
@with_secondary(start=False)
@with_aktualizr(start=False, run_mode='once', output_logs=True)
def test_secondary_ostree_update(uptane_repo, secondary, aktualizr, treehub, sysroot, director, **kwargs):
    """Test Secondary ostree update if a boot order of Secondary and Primary is undefined"""

    target_rev = treehub.revision
    expected_targetname = uptane_repo.add_ostree_target(secondary.id, target_rev, "GARAGE_TARGET_NAME")

    with secondary:
        with aktualizr:
            aktualizr.wait_for_completion()

    pending_rev = aktualizr.get_current_pending_image_info(secondary.id)

    if pending_rev != target_rev:
        logger.error("Pending version {} != the target one {}".format(pending_rev, target_rev))
        return False

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

    if expected_targetname != director.get_ecu_manifest_filepath(secondary.id[1]):
        logger.error(
            "Target name doesn't match a filepath value of the reported manifest: expected: {}, actual: {}".
            format(expected_targetname, director.get_ecu_manifest_filepath(secondary.id[1])))
        return False

    return True


@with_uptane_backend()
@with_secondary(start=False)
@with_aktualizr(start=False, output_logs=False, wait_timeout=0.1)
def test_primary_timeout_during_first_run(uptane_repo, secondary, aktualizr, **kwargs):
    """Test Aktualizr's timeout of waiting for Secondaries during initial boot"""

    secondary_image_filename = "secondary_image_filename_001.img"
    secondary_image_hash = uptane_repo.add_image(id=secondary.id, image_filename=secondary_image_filename)

    logger.debug("Checking Aktualizr behaviour if it timeouts while waiting for a connection from the secondary")

    # just start the aktualizr and expect that it timeouts on waiting for a connection from the secondary
    # so the secondary is not registered at the device and backend
    with aktualizr:
        aktualizr.wait_for_completion()

    info = aktualizr.get_info()
    if info is None:
        return False
    not_provisioned = 'Provisioned on server: no' in info

    return not_provisioned and not aktualizr.is_ecu_registered(secondary.id)


@with_uptane_backend()
@with_director()
@with_secondary(start=False)
@with_aktualizr(start=False, output_logs=True)
def test_primary_wait_secondary_install(uptane_repo, secondary, aktualizr, director, **kwargs):
    """Test that Primary waits for Secondary to connect before installing"""

    # provision device with a secondary
    with secondary, aktualizr:
        aktualizr.wait_for_completion()

    secondary_image_filename = "secondary_image_filename.img"
    secondary_image_hash = uptane_repo.add_image(id=secondary.id, image_filename=secondary_image_filename)

    with aktualizr:
        time.sleep(10)
        with secondary:
            aktualizr.wait_for_completion()

    if not director.get_install_result():
        logger.error("Installation result is not successful")
        return False

    return True


@with_uptane_backend()
@with_secondary(start=False)
@with_aktualizr(start=False, output_logs=False)
def test_primary_timeout_after_device_is_registered(uptane_repo, secondary, aktualizr, **kwargs):
    '''Test Aktualizr's timeout of waiting for Secondaries after the device/aktualizr was registered at the backend'''

    # run aktualizr and secondary and wait until the device/aktualizr is registered
    with aktualizr, secondary:
        aktualizr.wait_for_completion()

    # the secondary must be registered
    if not aktualizr.is_ecu_registered(secondary.id):
        return False

    # make sure that the secondary is not running
    if secondary.is_running():
        return False

    # run just aktualizr, the previously registered secondary is off
    # and check if the primary ECU is updatable if the secondary is not connected
    primary_image_filename = "primary_image_filename_001.img"
    primary_image_hash = uptane_repo.add_image(id=aktualizr.id, image_filename=primary_image_filename)

    # if a new image for the not-connected secondary is specified in the target
    # then nothing is going to be updated, including the image intended for
    # healthy primary ECU
    # secondary_image_filename = "secondary_image_filename_001.img"
    # secondary_image_hash = uptane_repo.add_image(id=secondary.id, image_filename=secondary_image_filename)

    aktualizr.update_wait_timeout(0.1)
    with aktualizr:
        aktualizr.wait_for_completion()

    return aktualizr.get_current_primary_image_info() == primary_image_hash


# test suit runner
if __name__ == '__main__':
    logging.basicConfig(level=logging.INFO)

    parser = argparse.ArgumentParser(description='Test IP Secondary')
    parser.add_argument('-b', '--build-dir', help='build directory', default='build')
    parser.add_argument('-s', '--src-dir', help='source directory', default='.')
    parser.add_argument('-o', '--ostree', help='ostree support', default='OFF')

    input_params = parser.parse_args()

    KeyStore.base_dir = path.abspath(input_params.src_dir)
    initial_cwd = getcwd()
    chdir(input_params.build_dir)

    test_suite = [
                    test_secondary_update,
                    test_secondary_update_if_secondary_starts_first,
                    test_secondary_update_if_primary_starts_first,
                    test_primary_timeout_during_first_run,
                    test_primary_timeout_after_device_is_registered
    ]

    if input_params.ostree == 'ON':
        test_suite.append(test_secondary_ostree_update)

    test_suite_run_result = True
    for test in test_suite:
        logger.info('>>> Running {}...'.format(test.__name__))
        test_run_result = test()
        logger.info('>>> {}: {}\n'.format('OK' if test_run_result else 'FAILED', test.__name__))
        test_suite_run_result = test_suite_run_result and test_run_result

    chdir(initial_cwd)
    exit(0 if test_suite_run_result else 1)

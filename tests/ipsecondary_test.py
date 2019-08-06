#!/usr/bin/env python3

import tempfile
import subprocess
import logging
import argparse

from os import path
from os import devnull
from os import getcwd, chdir
from uuid import uuid4
from functools import wraps

from test_fixtures import with_aktualizr, with_uptane_backend, KeyStore
from fake_http_server.fake_test_server import FakeTestServerBackground

logger = logging.getLogger("IPSecondaryTest")


class IPSecondary:

    def __init__(self, aktualizr_secondary_exe, id, port=9050, primary_port=9040):
        self.id = id
        self.port = port

        self._aktualizr_secondary_exe = aktualizr_secondary_exe
        self._storage_dir = tempfile.TemporaryDirectory()

        with open(path.join(self._storage_dir.name, 'config.toml'), 'w+') as config_file:
            config_file.write(IPSecondary.CONFIG_TEMPLATE.format(serial=id[1], hw_ID=id[0],
                                                                 port=port, primary_port=primary_port,
                                                                 storage_dir=self._storage_dir,
                                                                 db_path=path.join(self._storage_dir.name, 'db.sql')))
            self._config_file = config_file.name

    CONFIG_TEMPLATE = '''
    [uptane]
    ecu_serial = "{serial}"
    ecu_hardware_id = "{hw_ID}"

    [network]
    port = {port}
    primary_ip = "127.0.0.1"
    primary_port = {primary_port}

    [storage]
    type = "sqlite"
    path = "{storage_dir}"
    sqldb_path = "{db_path}"


    [pacman]
    type = "fake"
    '''

    def is_running(self):
        return True if self._process.poll() is None else False

    def __enter__(self):
        self._process = subprocess.Popen([self._aktualizr_secondary_exe, '-c', self._config_file],
                                         stdout=open(devnull, 'w'), close_fds=True)
        logger.debug("IP Secondary {} has been started: {}".format(self.id, self.port))
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self._process.terminate()
        self._process.wait(timeout=10)
        logger.debug("IP Secondary {} has been stopped".format(self.id))


def with_secondary(start=True, id=('secondary-hw-ID-001', str(uuid4())),
                   aktualizr_secondary_exe='src/aktualizr_secondary/aktualizr-secondary'):
    def decorator(test):
        @wraps(test)
        def wrapper(*args, **kwargs):
            secondary = IPSecondary(aktualizr_secondary_exe=aktualizr_secondary_exe, id=id)
            if start:
                with secondary:
                    result = test(*args, **kwargs, secondary=secondary)
            else:
                result = test(*args, **kwargs, secondary=secondary)
            return result
        return wrapper
    return decorator


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

    test_result = (secondary_image_hash, secondary_image_filename) == aktualizr.get_current_image_info(secondary.id)
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

    test_result = (secondary_image_hash, secondary_image_filename) == aktualizr.get_current_image_info(secondary.id)
    logger.debug("Update result: {}".format("success" if test_result else "failed"))
    return test_result


@with_uptane_backend()
@with_secondary(start=False)
@with_aktualizr(start=False, output_logs=False)
def test_secondary_update(uptane_repo, secondary, aktualizr, **kwargs):
    '''Test Secondary update if a boot order of Secondary and Primary is undefined'''

    test_result = True
    number_of_updates = 1
    ii = 0
    while ii < number_of_updates and test_result:
        # add a new image to the repo in order to update the secondary with it
        secondary_image_filename = "secondary_image_filename_{}.img".format(ii)
        secondary_image_hash = uptane_repo.add_image(id=secondary.id, image_filename=secondary_image_filename)

        logger.debug("Trying to update ECU {} with the image {}".
                    format(secondary.id, (secondary_image_hash, secondary_image_filename)))

        # start Secondary and Aktualizr processes, aktualizr is started in 'once' mode
        with secondary, aktualizr:
            aktualizr.wait_for_completion()

        test_result = (secondary_image_hash, secondary_image_filename) == aktualizr.get_current_image_info(secondary.id)
        logger.debug("Update result: {}".format("success" if test_result else "failed"))
        ii += 1

    return test_result


@with_uptane_backend()
@with_secondary(start=False)
@with_aktualizr(start=False, output_logs=False, wait_timeout=0.1)
def test_primary_timeout_during_first_run(uptane_repo, secondary, aktualizr, **kwargs):
    '''Test Aktualizr's timeout of waiting for Secondaries during initial boot'''

    secondary_image_filename = "secondary_image_filename_001.img"
    secondary_image_hash = uptane_repo.add_image(id=secondary.id, image_filename=secondary_image_filename)

    logger.debug("Checking Aktualizr behaviour if it timeouts while waiting for a connection from the secondary")

    # just start the aktualizr and expect that it timeouts on waiting for a connection from the secondary
    # so the secondary is not registered at the device and backend
    with aktualizr:
        aktualizr.wait_for_completion()

    return not aktualizr.is_ecu_registered(secondary.id)


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

    return (aktualizr.get_current_primary_image_info() == primary_image_hash)\
            and not aktualizr.is_ecu_registered(secondary.id)


# test suit runner
if __name__ == '__main__':
    logging.basicConfig(level=logging.INFO)

    parser = argparse.ArgumentParser(description='Test IP Secondary')
    parser.add_argument('-b', '--build-dir', help='build directory', default='build')
    parser.add_argument('-s', '--src-dir', help='source directory', default='.')
    input_params = parser.parse_args()

    KeyStore.base_dir = input_params.src_dir
    initial_cwd = getcwd()
    chdir(input_params.build_dir)

    test_suite = [test_secondary_update_if_secondary_starts_first,
                  test_secondary_update_if_primary_starts_first,
                  test_secondary_update,
                  test_primary_timeout_during_first_run,
                  test_primary_timeout_after_device_is_registered]

    test_suite_run_result = True
    for test in test_suite:
        logger.info('>>> Running {}...'.format(test.__name__))
        test_run_result = test()
        logger.info('>>> {}: {}'.format('OK' if test_run_result else 'Failed', test.__name__))
        test_suite_run_result = test_suite_run_result and test_run_result

    chdir(initial_cwd)
    exit(0 if test_suite_run_result else 1)

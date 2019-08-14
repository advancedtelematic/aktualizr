#!/usr/bin/env python3

import tempfile
import subprocess
import json
import logging
import argparse

from os import path
from os import urandom
from os import devnull
from os import getcwd, chdir
from uuid import uuid4
from functools import wraps

from fake_http_server.fake_test_server import FakeTestServerBackground

logger = logging.getLogger("IPSecondaryTest")


# The following are candidates for fixtures/common/shared components that can be reused by
# multiple tests/test suits
class UptaneTestRepo:

    def __init__(self, repo_manager_exe):
        self.image_rel_dir = 'repo/image'
        self.target_rel_dir = 'repo/image/targets'

        self._repo_manager_exe = repo_manager_exe
        self._repo_root_dir = tempfile.TemporaryDirectory()

    @property
    def root_dir(self):
        return self._repo_root_dir.name

    @property
    def image_dir(self):
        return path.join(self.root_dir, self.image_rel_dir)

    @property
    def target_dir(self):
        return path.join(self.root_dir, self.target_rel_dir)

    @property
    def target_file(self):
        return path.join(self.image_dir, 'targets.json')

    def add_image(self, id, image_filename, target_name=None, image_size=1024):

        targetname = target_name if target_name else image_filename

        with open(path.join(self.image_dir, image_filename), 'wb') as image_file:
            image_file.write(urandom(image_size))

        subprocess.run([self._repo_manager_exe, '--path', self.root_dir,
                        '--command', 'image', '--filename', image_filename, '--targetname', targetname, '--hwid', id[0]],
                       cwd=self.image_dir, check=True)

        # update the director metadata
        subprocess.run([self._repo_manager_exe, '--path', self.root_dir,
                        '--command', 'addtarget', '--targetname', targetname,
                        '--hwid', id[0], '--serial', id[1]], check=True)

        # sign so the image becomes available for an update for a client/device
        subprocess.run([self._repo_manager_exe, '--path', self.root_dir, '--command', 'signtargets'], check=True)

        with open(self.target_file, "r") as target_file:
            targets = json.load(target_file)
            target_hash = targets["signed"]["targets"][targetname]["hashes"]["sha256"]

        return target_hash

    def __enter__(self):
        self._generate_repo()
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self._repo_root_dir.cleanup()

    def _generate_repo(self):
        subprocess.run([self._repo_manager_exe, '--path', self.root_dir, '--command', 'generate'], check=True)


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


class Aktualizr:

    def __init__(self, aktualizr_primary_exe, aktualizr_info_exe, id,
                 server_url, ca, pkey, cert, wait_port=9040, wait_timeout=60, secondary=None, output_logs=True):
        self.id = id

        self._aktualizr_primary_exe = aktualizr_primary_exe
        self._aktualizr_info_exe = aktualizr_info_exe
        self._storage_dir = tempfile.TemporaryDirectory()

        with open(path.join(self._storage_dir.name, 'secondary_config.json'), 'w+') as secondary_config_file:
            secondary_cfg = json.loads(Aktualizr.SECONDARY_CONFIG_TEMPLATE.format(port=wait_port, timeout=wait_timeout))
            json.dump(secondary_cfg, secondary_config_file)
            self._secondary_config_file = secondary_config_file.name

        with open(path.join(self._storage_dir.name, 'config.toml'), 'w+') as config_file:
            config_file.write(Aktualizr.CONFIG_TEMPLATE.format(server_url=server_url,
                                                               ca_path=ca, pkey_path=pkey, cert_path=cert,
                                                               serial=id[1], hw_ID=id[0],
                                                               storage_dir=self._storage_dir,
                                                               db_path=path.join(self._storage_dir.name, 'db.sql'),
                                                               secondary_cfg_file=self._secondary_config_file))
            self._config_file = config_file.name

        self.add_secondary(secondary) if secondary else None
        self._output_logs = output_logs

    CONFIG_TEMPLATE = '''
    [tls]
    server = "{server_url}"

    [import]
    tls_cacert_path = "{ca_path}"
    tls_pkey_path = "{pkey_path}"
    tls_clientcert_path = "{cert_path}"

    [provision]
    primary_ecu_serial = "{serial}"
    primary_ecu_hardware_id = "{hw_ID}"

    [storage]
    path = "{storage_dir}"
    type = "sqlite"
    sqldb_path = "{db_path}"

    [pacman]
    type = "fake"

    [uptane]
    secondary_config_file = "{secondary_cfg_file}"

    [logger]
    loglevel = 1

    '''

    SECONDARY_CONFIG_TEMPLATE = '''
    {{
      "IP": {{
        "secondaries_wait_port": {port},
        "secondaries_wait_timeout": {timeout},
        "secondaries": []
      }}
    }}
    '''

    def add_secondary(self, secondary):
        with open(self._secondary_config_file, "r+") as config_file:
            sec_cfg = json.load(config_file)
            sec_cfg["IP"]["secondaries"].append({"addr": "127.0.0.1:{}".format(secondary.port)})
            config_file.seek(0)
            json.dump(sec_cfg, config_file)

    def update_wait_timeout(self, timeout):
        with open(self._secondary_config_file, "r+") as config_file:
            sec_cfg = json.load(config_file)
            sec_cfg["IP"]["secondaries_wait_timeout"] = timeout
            config_file.seek(0)
            json.dump(sec_cfg, config_file)

    def run(self, run_mode):
        subprocess.run([self._aktualizr_primary_exe, '-c', self._config_file, '--run-mode', run_mode], check=True)

    def get_info(self):
        return str(subprocess.check_output([self._aktualizr_info_exe, '-c', self._config_file]))

    # ugly stuff that could be removed if Aktualizr had exposed API to check status
    # or aktializr-info had output status/info in a structured way (e.g. json)
    def is_ecu_registered(self, ecu_id):
        device_status = self.get_info()
        if not ((device_status.find(ecu_id[0]) != -1) and (device_status.find(ecu_id[1]) != -1)):
            return False
        not_registered_field = "Removed or not registered ecus:"
        not_reg_start = device_status.find(not_registered_field)
        return not_reg_start == -1 or (device_status.find(ecu_id[1], not_reg_start) == -1)

    # applicable only to secondary ECUs due to inconsistency in presenting information
    # about primary and secondary ECUs
    # ugly stuff that could be removed if Aktualizr had exposed API to check status
    # or aktializr-info had output status/info in a structured way (e.g. json)
    def get_current_image_info(self, ecu_id):
        secondary_image_hash_field = 'installed image hash: '
        secondary_image_filename_field = 'installed image filename: '
        aktualizr_status = self.get_info()
        ecu_serial = ecu_id[1]
        ecu_info_position = aktualizr_status.find(ecu_serial)
        if ecu_info_position == -1:
            return None

        start = aktualizr_status.find(secondary_image_hash_field, ecu_info_position)
        end = aktualizr_status.find('\\n', start)
        hash_val = aktualizr_status[start + len(secondary_image_hash_field):end]

        start = aktualizr_status.find(secondary_image_filename_field, ecu_info_position)
        end = aktualizr_status.find('\\n', start)
        filename_val = aktualizr_status[start + len(secondary_image_filename_field):end]

        return hash_val, filename_val

    # ugly stuff that could be removed if Aktualizr had exposed API to check status
    # or aktializr-info had output status/info in a structured way (e.g. json)
    def get_current_primary_image_info(self):
        primary_hash_field = 'Current primary ecu running version: '
        aktualizr_status = self.get_info()
        start = aktualizr_status.find(primary_hash_field)
        end = aktualizr_status.find('\\n', start)
        return aktualizr_status[start + len(primary_hash_field):end]

    def __enter__(self, run_mode='once'):
        self._process = subprocess.Popen([self._aktualizr_primary_exe, '-c', self._config_file, '--run-mode', run_mode],
                                         stdout=None if self._output_logs else open(devnull, 'w'), close_fds=True)
        logger.debug("Aktualizr has been started")
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self._process.terminate()
        self._process.wait(timeout=10)
        logger.debug("Aktualizr has been stopped")

    def wait_for_completion(self):
        self._process.wait(timeout=60)


# The following decorators can be eliminated if pytest framework (or similar) is used
# by using fixtures instead
def with_uptane_backend(repo_manager_exe='src/uptane_generator/uptane-generator'):
    def decorator(test):
        @wraps(test)
        def wrapper(*args, **kwargs):
            repo_manager_exe_abs_path = path.abspath(repo_manager_exe)
            with UptaneTestRepo(repo_manager_exe_abs_path) as repo, \
                    FakeTestServerBackground(meta_path=repo.root_dir,
                                             target_path=repo.target_dir) as uptane_server:
                result = test(*args, **kwargs, uptane_repo=repo, uptane_server=uptane_server)
            return result
        return wrapper
    return decorator


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


class KeyStore:
    base_dir = "./"

    @staticmethod
    def ca():
        return path.join(KeyStore.base_dir, 'tests/test_data/prov_selfupdate/ca.pem')

    @staticmethod
    def pkey():
        return path.join(KeyStore.base_dir, 'tests/test_data/prov_selfupdate/pkey.pem')

    @staticmethod
    def cert():
        return path.join(KeyStore.base_dir, 'tests/test_data/prov_selfupdate/client.pem')


def with_aktualizr(start=True, output_logs=True, id=('primary-hw-ID-001', str(uuid4())), wait_timeout=60,
                   aktualizr_primary_exe='src/aktualizr_primary/aktualizr',
                   aktualizr_info_exe='src/aktualizr_info/aktualizr-info'):
    def decorator(test):
        @wraps(test)
        def wrapper(*args, **kwargs):
            aktualizr = Aktualizr(aktualizr_primary_exe=aktualizr_primary_exe,
                           aktualizr_info_exe=aktualizr_info_exe,
                           id=id, ca=KeyStore.ca(), pkey=KeyStore.pkey(), cert=KeyStore.cert(),
                           server_url='http://localhost:{}'.format(kwargs['uptane_server'].port),
                           secondary=kwargs.get('secondary'), wait_timeout=wait_timeout, output_logs=output_logs)
            if start:
                with aktualizr:
                    result = test(*args, **kwargs, aktualizr=aktualizr)
            else:
                result = test(*args, **kwargs, aktualizr=aktualizr)
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

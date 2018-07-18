#!/usr/bin/env python3

import argparse
import os
import subprocess
import sys
import tempfile

from pathlib import Path
from time import sleep

import prov_test_common


def main():
    parser = argparse.ArgumentParser(description='Run a local implicit provisioning test with aktualizr')
    parser.add_argument('--build-dir', '-b', type=Path, default=Path('../build'), help='build directory')
    parser.add_argument('--credentials', '-c', type=Path, default=Path('.'), help='path to credentials archive')
    args = parser.parse_args()

    retval = 1
    with tempfile.TemporaryDirectory() as tmp_dir:
        retval = provision(Path(tmp_dir), args.build_dir, args.credentials)
    return retval


CONFIG_TEMPLATE = '''
[pacman]
type = "none"

[storage]
path = "{tmp_dir}"
type = "sqlite"
'''


def provision(tmp_dir, build_dir, creds):
    conf_dir = tmp_dir / 'conf.d'
    os.mkdir(str(conf_dir))
    conf_prov = conf_dir / '20-implicit_prov.toml'
    conf_server = conf_dir / '30-implicit_server.toml'
    with conf_prov.open('w') as f:
        f.write(CONFIG_TEMPLATE.format(tmp_dir=tmp_dir))
    akt = build_dir / 'src/aktualizr_primary/aktualizr'
    akt_info = build_dir / 'src/aktualizr_info/aktualizr-info'
    akt_iw = build_dir / 'src/implicit_writer/aktualizr_implicit_writer'
    akt_cp = build_dir / 'src/cert_provider/aktualizr_cert_provider'

    # Run implicit_writer (equivalent to aktualizr-implicit-prov.bb).
    cacert_path = tmp_dir / 'root.crt'
    stdout, stderr, retcode = prov_test_common.run_subprocess([str(akt_iw),
        '-c', str(creds), '-o', str(conf_server), '-r', str(cacert_path)])
    if retcode > 0:
        print('aktualizr_implicit_writer failed (' + str(retcode) + '): ' +
                stderr.decode() + stdout.decode())
        return retcode

    with subprocess.Popen([str(akt), '--config', str(conf_dir)]) as proc:
        try:
            # Verify that device has NOT yet provisioned.
            for delay in [1, 2, 5, 10, 15]:
                sleep(delay)
                stdout, stderr, retcode = prov_test_common.run_subprocess([str(akt_info),
                    '--config', str(conf_dir)])
                if retcode == 0 and stderr == b'':
                    break
            if (b'Couldn\'t load device ID' not in stdout or
                    b'Couldn\'t load ECU serials' not in stdout or
                    b'Provisioned on server: no' not in stdout or
                    b'Fetched metadata: no' not in stdout):
                print('Device already provisioned!? ' + stderr.decode() + stdout.decode())
                return 1
        finally:
            proc.kill()

    # Run cert_provider.
    print('Device has not yet provisioned (as expected). Running cert_provider.')
    stdout, stderr, retcode = prov_test_common.run_subprocess([str(akt_cp),
        '-c', str(creds), '-l', str(tmp_dir), '-s', '-g', str(conf_prov)])
    if retcode > 0:
        print('aktualizr_cert_provider failed (' + str(retcode) + '): ' +
                stderr.decode() + stdout.decode())
        return retcode

    r = 1
    with subprocess.Popen([str(akt), '--config', str(conf_dir)]) as proc:
        try:
            r = prov_test_common.verify_provisioned(akt_info, conf_dir)
        finally:
            proc.kill()
    return r


if __name__ == '__main__':
    sys.exit(main())

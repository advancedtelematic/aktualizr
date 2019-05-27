#!/usr/bin/env python3

import argparse
import os
import sys
import tempfile

from pathlib import Path

from prov_test_common import run_subprocess, verify_provisioned


def main():
    parser = argparse.ArgumentParser(description='Run a local device credential provisioning test with aktualizr')
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

[tls]
server_url_path = "{tmp_dir}/gateway.url"

[storage]
path = "{tmp_dir}"
type = "sqlite"

[import]
base_path = "{tmp_dir}/import"
tls_cacert_path = "root.crt"
tls_clientcert_path = "client.pem"
tls_pkey_path = "pkey.pem"
'''


def provision(tmp_dir, build_dir, creds):
    conf_dir = tmp_dir / 'conf.d'
    os.mkdir(str(conf_dir))
    conf_prov = conf_dir / '20-device-cred-prov.toml'
    with conf_prov.open('w') as f:
        f.write(CONFIG_TEMPLATE.format(tmp_dir=tmp_dir))
    akt = build_dir / 'src/aktualizr_primary/aktualizr'
    akt_info = build_dir / 'src/aktualizr_info/aktualizr-info'
    akt_cp = build_dir / 'src/cert_provider/aktualizr-cert-provider'

    akt_input = [str(akt), '--config', str(conf_dir), '--run-mode', 'once']
    run_subprocess(akt_input)
    # Verify that device has NOT yet provisioned.
    stdout, stderr, retcode = run_subprocess([str(akt_info), '--config', str(conf_dir)])
    if (b'Couldn\'t load device ID' not in stdout or
            b'Couldn\'t load ECU serials' not in stdout or
            b'Provisioned on server: no' not in stdout or
            b'Fetched metadata: no' not in stdout):
        print('Device already provisioned!? ' + stderr.decode() + stdout.decode())
        return 1

    # Run aktualizr-cert-provider.
    print('Device has not yet provisioned (as expected). Running aktualizr-cert-provider.')
    stdout, stderr, retcode = run_subprocess([str(akt_cp),
        '-c', str(creds), '-l', '/', '-s', '-u', '-r', '-g', str(conf_prov)])
    if retcode > 0:
        print('aktualizr-cert-provider failed (' + str(retcode) + '): ' +
              stderr.decode() + stdout.decode())
        return retcode

    run_subprocess(akt_input)
    return verify_provisioned(akt_info, conf_dir)


if __name__ == '__main__':
    sys.exit(main())

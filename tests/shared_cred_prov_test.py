#!/usr/bin/env python3

import argparse
import subprocess
import sys
import tempfile

from pathlib import Path

from prov_test_common import run_subprocess, verify_provisioned


def main():
    parser = argparse.ArgumentParser(description='Run a local shared device provisioning test with aktualizr')
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

[provision]
provision_path = "{creds}"
mode = "SharedCredReuse"

[storage]
path = "{tmp_dir}"
type = "sqlite"
sqldb_path = "{db}"
'''


def provision(tmp_dir, build_dir, creds):
    db = tmp_dir / 'sql.db'
    conf = tmp_dir / '20-shared-cred-prov.toml'
    with conf.open('w') as f:
        f.write(CONFIG_TEMPLATE.format(creds=creds, tmp_dir=tmp_dir, db=db))
    akt = build_dir / 'src/aktualizr_primary/aktualizr'
    akt_info = build_dir / 'src/aktualizr_info/aktualizr-info'

    run_subprocess([str(akt), '--config', str(conf), '--run-mode', 'once'])
    return verify_provisioned(akt_info, conf)


if __name__ == '__main__':
    sys.exit(main())

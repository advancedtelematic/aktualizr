#!/usr/bin/env python3

import argparse
import platform
import re
import shutil
import subprocess
import sys
import tempfile

from pathlib import Path
from time import sleep


def main():
    parser = argparse.ArgumentParser(description='Run a local provisioning test with aktualizr')
    parser.add_argument('--build-dir', '-b', type=Path, default=Path('../build'), help='build directory')
    parser.add_argument('--src-dir', '-s', type=Path, default=Path('../'), help='source directory (parent of src/)')
    parser.add_argument('--credentials', '-c', type=Path, default=Path('.'), help='path to credentials archive')
    args = parser.parse_args()

    retval = 1
    with tempfile.TemporaryDirectory() as tmp_dir:
        retval = provision(Path(tmp_dir), args.build_dir, args.src_dir, args.credentials)
    return retval


CONFIG_TEMPLATE = '''
        [pacman]
        type = "none"

        [provision]
        provision_path = "{creds}"

        [storage]
        path = "{tmp_dir}"
        type = "sqlite"
        sqldb_path = "{db}"
'''


def provision(tmp_dir, build_dir, src_dir, creds_in):
    creds = tmp_dir / creds_in.name
    shutil.copyfile(str(creds_in), str(creds))
    db = tmp_dir / 'sql.db'
    conf = tmp_dir / 'config.toml'
    with conf.open('w') as f:
        f.write(CONFIG_TEMPLATE.format(creds=creds, tmp_dir=tmp_dir, db=db))
    akt = build_dir / 'src/aktualizr_primary/aktualizr'
    akt_info = build_dir / 'src/aktualizr_info/aktualizr-info'

    with subprocess.Popen([str(akt), '--config', str(conf)]) as proc:
        try:
            # Verify that device HAS provisioned.
            for delay in [5, 5, 5, 5, 10, 10, 10, 10]:
                sleep(delay)
                stdout, stderr, retcode = run_aktualizr_info([str(akt_info), '--config', str(conf)])
                if retcode == 0 and stderr == b'' and 'Fetched metadata: yes' in stdout.decode():
                    break
            machine = platform.node()
            if (b'Device ID: ' not in stdout or
                    b'Primary ecu hardware ID: ' + machine.encode() not in stdout or
                    b'Fetched metadata: yes' not in stdout):
                print('Provisioning failed: ' + stderr.decode() + stdout.decode())
                return 1
            p = re.compile(r'Device ID: ([a-z0-9-]*)\n')
            m = p.search(stdout.decode())
            if not m or m.lastindex <= 0:
                print('Device ID could not be read: ' + stderr.decode() + stdout.decode())
                return 1
            print('Device successfully provisioned with ID: ' + m.group(1))
        finally:
            proc.kill()
    return 0


def run_aktualizr_info(command):
    s = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=True)
    return s.stdout, s.stderr, s.returncode


if __name__ == '__main__':
    sys.exit(main())

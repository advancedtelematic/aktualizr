#!/usr/bin/env python3

import argparse
import hashlib
import os.path
import requests
import shutil
import sys
import tarfile
import xml.etree.ElementTree as ET

from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description='Download a specific or the latest version of garage-sign')
    parser.add_argument('-n', '--name', help='specific version to download')
    parser.add_argument('-s', '--sha256', help='expected hash of requested version')
    parser.add_argument('-o', '--output', type=Path, default=Path('.'), help='download directory')
    args = parser.parse_args()
    if not args.output.exists():
        print('Error: specified output directory ' + args.output + ' does not exist!')
        return 1
    sha256_hash = args.sha256
    if sha256_hash and not args.name:
        print('Warning: sha256 hash specified without specifying a version.')
    if args.name and not sha256_hash:
        print('Warning: specific version requested without specifying the sha256 hash.')

    r = requests.get('https://ats-tuf-cli-releases.s3-eu-central-1.amazonaws.com')
    if r.status_code != 200:
        print('Error: unable to request index!')
        return 1
    tree = ET.fromstring(r.text)
    # Assume namespace.
    ns = '{http://s3.amazonaws.com/doc/2006-03-01/}'
    items = tree.findall(ns + 'Contents')
    versions = dict()
    for i in (i for i in items if i.find(ns + 'Key').text[0:4] == 'cli-'):
        # ETag is md5sum.
        versions[i.find(ns + 'Key').text] = (i.find(ns + 'LastModified').text, i.find(ns + 'ETag').text[1:-1])
    if args.name:
        name = args.name
        if name not in versions:
            # Try adding tgz in case it wasn't included.
            name_ext = name + '.tgz'
            if name_ext not in versions:
                print('Error: ' + name + ' not found in tuf-cli releases.')
                return 1
            else:
                name = name_ext
    else:
        name = sorted(versions)[-1]
    path = args.output.joinpath(name)
    md5_hash = versions[name][1]
    if not path.is_file() or not check_hashes(name, path, md5_hash, sha256_hash):
        print('Downloading ' + name + ' from server...')
        if download(name, path, md5_hash, sha256_hash):
            print(name + ' successfully downloaded and validated.')
        else:
            return 1
    else:
        print(name + ' already present and validated.')

    # Remove anything leftover inside the extracted directory.
    extract_path = args.output.joinpath('garage-sign')
    if extract_path.exists():
        for f in os.listdir(str(extract_path)):
            shutil.rmtree(str(extract_path.joinpath(f)))
    # Always extract everything.
    t = tarfile.open(str(path))
    t.extractall(path=str(args.output))
    return 0


def download(name, path, md5_hash, sha256_hash):
    r2 = requests.get('https://ats-tuf-cli-releases.s3-eu-central-1.amazonaws.com/' + name, stream=True)
    if r2.status_code != 200:
        print('Error: unable to request file!')
        return False
    with path.open(mode='wb') as f:
        shutil.copyfileobj(r2.raw, f)
    return check_hashes(name, path, md5_hash, sha256_hash)


def check_hashes(name, path, md5_hash, sha256_hash):
    if not tarfile.is_tarfile(str(path)):
        print('Error: ' + name + ' is not a valid tar archive!')
        return False
    m = hashlib.md5()
    if sha256_hash:
        s = hashlib.sha256()
    with path.open(mode='rb') as f:
        data = f.read()
        m.update(data)
        if sha256_hash:
            s.update(data)
    if m.hexdigest() != md5_hash:
        print('Error: md5 hash of ' + name + ' does not match expected value!')
        print(m.hexdigest())
        print(md5_hash)
        return False
    if sha256_hash and s.hexdigest() != sha256_hash:
        print('Error: sha256 hash of ' + name + ' does not match provided value!')
        return False
    return True


if __name__ == '__main__':
    sys.exit(main())

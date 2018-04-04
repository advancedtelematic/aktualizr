#!/usr/bin/env python3

import argparse
import hashlib
import os.path
import requests
import shutil
import sys
import xml.etree.ElementTree as ET

from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description='Download a specific or the latest version of garage-sign')
    parser.add_argument('-n', '--name', help='specific version to download')
    parser.add_argument('-o', '--output', type=Path, default=Path('.'), help='download directory')
    args = parser.parse_args()
    if not args.output.exists():
        print('Error: specified output directory ' + args.output + ' does not exist!')
        return 1

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
            print('Error: ' + name + ' not found in tuf-cli releases.')
            return 1
    else:
        name = sorted(versions)[-1]
    path = args.output.joinpath(name)
    md5_hash = versions[name][1]
    m = hashlib.md5()
    if os.path.isfile(path):
        if check_md5(path, m, md5_hash):
            print(name + ' already present with valid md5 hash.')
            return 0
        else:
            print('md5 hash of local ' + name + ' doesn\'t match expected value, downloading from server...')

    r2 = requests.get('https://ats-tuf-cli-releases.s3-eu-central-1.amazonaws.com/' + name, stream=True)
    if r2.status_code != 200:
        print('Error: unable to request file!')
        return 1
    with open(path, mode='wb') as f:
        shutil.copyfileobj(r2.raw, f)
    if not check_md5(path, m, md5_hash):
        print('Error: md5 hash of ' + name + ' does not match expected value!')
        return 1
    else:
        print(name + ' successfully downloaded with valid md5 hash.')
        return 0


def check_md5(path, m, md5_hash):
    with open(path, mode='rb') as f:
        m.update(f.read())
    return m.hexdigest() == md5_hash


if __name__ == '__main__':
    sys.exit(main())

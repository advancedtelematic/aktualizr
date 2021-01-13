#!/usr/bin/env python3

import argparse
import hashlib
import os.path
import shutil
import sys
import tarfile
import urllib.request
import xml.etree.ElementTree as ET

from pathlib import Path


#aws_bucket_url = 'https://tuf-cli-releases.ota.here.com/'
aws_bucket_url = 'https://storage.googleapis.com/public-shared-artifacts-fio/mirrors/ota-tuf-cli-releases/'
default_version_name = 'cli-0.7.2-48-gf606131.tgz'
default_version_sha256 = 'f20c9f3e08fff277a78786025105298322c54874ca66a753e7ad0b2ffb239502'
default_version_size = '59517659'


def main():
    parser = argparse.ArgumentParser(description='Download a specific or the latest version of garage-sign')
    parser.add_argument('-a', '--archive', help='static local archive')
    parser.add_argument('-n', '--name', help='specific version to download', default=default_version_name)
    parser.add_argument('-s', '--sha256', help='expected hash of requested version', default=default_version_sha256)
    parser.add_argument('-o', '--output', type=Path, default=Path('.'), help='download directory')
    args = parser.parse_args()

    if not args.output.exists():
        print('Error: specified output directory ' + args.output + ' does not exist!')
        return 1

    if args.archive:
        path = args.archive
    else:
        path = download_garage_cli_tools(args.name, args.sha256, args.output)
        if path is None:
            return 1

    # Remove anything leftover inside the extracted directory.
    extract_path = args.output.joinpath('garage-sign')
    if extract_path.exists():
        for f in os.listdir(str(extract_path)):
            shutil.rmtree(str(extract_path.joinpath(f)))
    # Always extract everything.
    t = tarfile.open(str(path))
    t.extractall(path=str(args.output))
    return 0


def find_version(version_name, sha256_hash, output):
    if sha256_hash and not version_name:
        print('Warning: sha256 hash specified without specifying a version.')
    if version_name and not sha256_hash:
        print('Warning: specific version requested without specifying the sha256 hash.')

    r = urllib.request.urlopen(aws_bucket_url)
    if r.status != 200:
        print('Error: unable to request index!')
        return None
    tree = ET.fromstring(r.read().decode('utf-8'))
    # Assume namespace.
    ns = '{http://s3.amazonaws.com/doc/2006-03-01/}'
    items = tree.findall(ns + 'Contents')
    versions = dict()
    cli_items = [i for i in items if i.find(ns + 'Key').text.startswith('cli-')]
    for i in cli_items:
        versions[i.find(ns + 'Key').text] = (i.find(ns + 'LastModified').text,
                                             i.find(ns + 'Size').text)
    if version_name:
        name = version_name
        if name not in versions:
            # Try adding tgz in case it wasn't included.
            name_ext = name + '.tgz'
            if name_ext not in versions:
                print('Error: ' + name + ' not found in tuf-cli releases.')
                return None
            else:
                name = name_ext
    else:
        name = max(versions, key=(lambda name: (versions[name][0])))

    path = output.joinpath(name)
    size = versions[name][1]
    if not path.is_file() or not verify(name, path, size, sha256_hash):
        print('Downloading ' + name + ' from server...')
        if download(name, path, size, sha256_hash):
            print(name + ' successfully downloaded and validated.')
            return path
        else:
            return None
    print(name + ' already present and validated.')
    return path


def download(name, path, size, sha256_hash):
    r = urllib.request.urlopen(aws_bucket_url + name)
    if r.status != 200:
        print('Error: unable to request file!')
        return False
    with path.open(mode='wb') as f:
        shutil.copyfileobj(r, f)
    return verify(name, path, size, sha256_hash)


def download_garage_cli_tools(name, sha256, output):
    path = output.joinpath(name)
    download_url = os.path.join(aws_bucket_url, name)
    print('Downloading ' + name + ' from server, url: {}...'.format(download_url))
    if download(name, path, default_version_size, sha256):
        print(name + ' successfully downloaded and validated.')
        return path
    else:
        return None


def verify(name, path, size, sha256_hash):
    if not tarfile.is_tarfile(str(path)):
        print('Error: ' + name + ' is not a valid tar archive!')
        return False
    actual_size = os.path.getsize(str(path))
    if actual_size != int(size):
        print('Error: size of ' + name + ' (' + str(actual_size) + ') does not match expected value (' + size + ')!')
        return False
    if sha256_hash:
        s = hashlib.sha256()
        with path.open(mode='rb') as f:
            data = f.read()
            s.update(data)
        if s.hexdigest() != sha256_hash:
            print('Error: sha256 hash of ' + name + ' does not match provided value!')
            return False
    return True


if __name__ == '__main__':
    sys.exit(main())

#!/usr/bin/env python3

import hashlib
import requests
import shutil
import sys
import xml.etree.ElementTree as ET


def main():
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
        versions[i.find(ns + 'LastModified').text] = (i.find(ns + 'Key').text, i.find(ns + 'ETag').text[1:-1])
    latest = sorted(versions)[-1]
    name = versions[latest][0]
    md5_hash = versions[latest][1]
    r2 = requests.get('https://ats-tuf-cli-releases.s3-eu-central-1.amazonaws.com/' + name, stream=True)
    if r2.status_code != 200:
        print('Error: unable to request file!')
        return 1
    with open(name, mode='wb') as f:
        shutil.copyfileobj(r2.raw, f)
    m = hashlib.md5()
    with open(name, mode='rb') as f:
        m.update(f.read())
    if m.hexdigest() != md5_hash:
        print('Error: md5 hash does not match!')
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())

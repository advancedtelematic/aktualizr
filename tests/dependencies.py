#! /usr/bin/python3

"""
A machine readable list of the dependencies of aktualizr.

This module also includes a command-line interface to validate this list
against the output of 'ldd' run against a built executable.

Use this like:

./dependencies.py ../build/src/aktualizr
"""

import subprocess
import sys
import re


class Dep:
    """A dependency"""
    def __init__(self, name, license=None, other_names=[], build_flag=None):
        self.name = name
        self.license = license
        self.other_names = other_names
        self.build_flag = build_flag

_direct_deps = [
    # Direct dependencies
    Dep('boost', 'Boost', ['boost1.62']),
    Dep('curl', 'MIT'),
    Dep('dbus', 'AFL-2 | GPLv2+', build_flag='BUILD_GENIVI'),
    Dep('glib2.0', 'LGPLv2+', build_flag='BUILD_GENIVI'),
    Dep('jansson', 'MIT', build_flag='BUILD_GENIVI'),
    Dep('jsoncpp', 'MIT'),
    Dep('libarchive', 'New BSD'),
    Dep('libsodium', 'ISC'),
    Dep('openssl', 'openssl'),
    Dep('ostree', 'LGPLv2'),
    Dep('picojson', 'BSD-2-Clause'),
    Dep('rvi_lib', 'MPL', build_flag='BUILD_GENIVI'),
]

_transitive_deps = [
    # Transitive dependencies
    Dep('acl'),
    Dep('attr'),
    Dep('bzip2'),
    Dep('cyrus-sasl2'),
    Dep('e2fsprogs'),
    Dep('gcc-6'),
    Dep('glibc'),
    Dep('gmp'),
    Dep('gnutls28'),
    Dep('gpgme1.0'),
    Dep('heimdal'),
    Dep('icu'),
    Dep('keyutils'),
    Dep('krb5'),
    Dep('libassuan'),
    Dep('libffi'),
    Dep('libgcc1'),
    Dep('libgcrypt20'),
    Dep('libgpg-error'),
    Dep('libidn'),
    Dep('libidn2-0'),
    Dep('libpsl'),
    Dep('libselinux'),
    Dep('libsoup2.4'),
    Dep('libtasn1-6'),
    Dep('libunistring'),
    Dep('libxml2'),
    Dep('lz4'),
    Dep('lzo2'),
    Dep('nettle'),
    Dep('openldap'),
    Dep('p11-kit'),
    Dep('pcre3'),
    Dep('rtmpdump'),
    Dep('sqlite3'),
    Dep('systemd'),
    Dep('util-linux'),
    Dep('xz-utils'),
    Dep('zlib'),
]

# Regular expressions for parsing the output of dpkg and ldd
re_lib = re.compile(b'\s+\S+ => ([^ ]*) \(0x[0-9a-f]+\)')
re_search = re.compile(b'([^:]+)')
re_source = re.compile(b'^Source: (\S+)$', flags=re.MULTILINE)


def ldd(executable):
    """
    Run ldd on executable, and return a list of the libraries it dynamically
    links to
    """
    ldd_output = subprocess.check_output(['ldd', executable])
    libs = []
    for lin in ldd_output.splitlines():
        m = re_lib.match(lin)
        if not m:
            print("NO match: %s" % lin)
        else:
            file = m.group(1)
            # print("Found file %s" % file)
            if file != b'':
                libs.append(file)
    return libs


def package_for_file(filepath):
    """Use dpkg --search to find the binary package that installed filepath"""
    # print ("looking up %s" % filepath)
    dpkg_output = subprocess.check_output(['dpkg', '--search', filepath])
    m = re_search.match(dpkg_output)
    return m.group(1)


def source_package_for_package(binary_package):
    """
    Map a binary Debian package name to its source package, if the two
    differ.  This uses apt-cache show and greps for a 'Source:' in the output.
    """
    apt_cache_output = subprocess.check_output(['apt-cache', 'show', binary_package])
    m = re_source.search(apt_cache_output)
    if not m:
        # print("No match for %s\n%s" % (binary_package, apt_cache_output))
        return binary_package.decode()
    return m.group(1).decode()


def get_source_packages(executable):
    """
    find the list of dynamic libraries that executable links to, then resolve
    those to Debian source packages
    """
    print("Finding dynamic libraries for %s..." % executable)
    libraries = ldd(executable)
    binary_packages = set()
    for l in libraries:
        binary_packages.add(package_for_file(l))
    # print(binary_packages)
    source_packages = set()
    for binary_package in binary_packages:
        source_packages.add(source_package_for_package(binary_package))
    return source_packages


def aktualizr_dependencies(all_names):
    """
    List the names of the packages that Aktualizr depends on. Note that the
    build flag information is currently ignored.
    """
    res = set()
    for d in _direct_deps + _transitive_deps:
        res.add(d.name)
        if all_names:
            res.update(set(d.other_names))
    return res


def check_dependencies(executable):
    declared_dependencies = aktualizr_dependencies(True)
    source_packages = get_source_packages(executable)
    missing = source_packages.difference(declared_dependencies)
    for m in missing:
        print("Missing dependency: %s" % m)
    else:
        print("All packages are accounted for")
        print("Aktualizr dependencies:")
        for n in sorted(aktualizr_dependencies(False)):
            print(n)

if __name__ == '__main__':
    check_dependencies(sys.argv[1])

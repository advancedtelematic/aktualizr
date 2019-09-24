#! /bin/bash

set -exuo pipefail

TEST_INSTALL_DESTDIR=${TEST_INSTALL_DESTDIR:-/persistent}

dpkg-deb -I "$TEST_INSTALL_DESTDIR"/aktualizr*.deb && dpkg -i "$TEST_INSTALL_DESTDIR"/aktualizr*.deb
akt_version=$(aktualizr --version)
(grep "$(cat "$TEST_INSTALL_DESTDIR"/aktualizr-version)" <<< "$akt_version") || (echo "$akt_version"; false)


#! /bin/bash

set -exuo pipefail

TEST_INSTALL_DESTDIR=${TEST_INSTALL_DESTDIR:-/persistent}

dpkg-deb -I "$TEST_INSTALL_DESTDIR"/aktualizr*.deb && dpkg -i "$TEST_INSTALL_DESTDIR"/aktualizr*.deb

export PATH=${PATH}:$TEST_INSTALL_DESTDIR

TMPDIR=$(mktemp -u)
create_repo.sh "$TMPDIR" 127.0.0.1
serve_repo.py 9000 "$TMPDIR" &

aktualizr --config "${TMPDIR}/sota.toml" --run-mode once

aktualizr-info --config "${TMPDIR}/sota.toml" | grep "Fetched metadata: yes" || exit 1

rm -r "$TMPDIR"

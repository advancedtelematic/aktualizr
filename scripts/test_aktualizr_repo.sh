#! /bin/bash

set -exuo pipefail

dpkg-deb -I /persistent/aktualizr.deb && dpkg -i /persistent/aktualizr.deb

export PATH=${PATH}:/persistent

TMPDIR=$(mktemp -u)
create_repo.sh $TMPDIR 127.0.0.1
serve_repo.py 9000 "$TMPDIR" &

aktualizr --config "${TMPDIR}/sota.toml" --running-mode once

{
aktualizr-info --config "${TMPDIR}/sota.toml" | grep "Fetched metadata: yes"
} || exit 1

rm -r $TMPDIR

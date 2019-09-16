#! /bin/bash

set -exuo pipefail

TEST_INSTALL_DESTDIR=${TEST_INSTALL_DESTDIR:-/persistent}

"$TEST_INSTALL_DESTDIR/selfupdate_server.py" 8000 "$TEST_INSTALL_DESTDIR"&

dpkg-deb -I "$TEST_INSTALL_DESTDIR"/aktualizr*.deb && dpkg -i "$TEST_INSTALL_DESTDIR"/aktualizr*.deb
akt_version=$(aktualizr --version)
(grep "$(cat "$TEST_INSTALL_DESTDIR"/aktualizr-version)" <<< "$akt_version") || (echo "$akt_version"; false)

TEMP_DIR=$(mktemp -d)
mkdir -m 700 -p "$TEMP_DIR/import"
cp "$TEST_INSTALL_DESTDIR"/prov_selfupdate/* "$TEMP_DIR/import"
echo -e "[storage]\\npath = \"$TEMP_DIR\"\\n[import]\\nbase_path = \"$TEMP_DIR/import\"" > "$TEMP_DIR/conf.toml"
aktualizr -c "$TEST_INSTALL_DESTDIR"/selfupdate.toml -c "$TEMP_DIR/conf.toml" once

# check that the version was updated
akt_version=$(aktualizr --version)
(grep 2.0-selfupdate <<< "$akt_version") || (echo "$akt_version"; false)

# check that aktualizr-info succeeds
aktualizr-info

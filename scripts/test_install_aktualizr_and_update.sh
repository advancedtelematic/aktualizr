#! /bin/bash

set -exuo pipefail

/persistent/selfupdate_server.py 8000&

dpkg-deb -I /persistent/aktualizr*.deb && dpkg -i /persistent/aktualizr*.deb
akt_version=$(aktualizr --version)
(grep "$(cat /persistent/aktualizr-version)" <<< "$akt_version") || (echo "$akt_version"; false)

TEMP_DIR=$(mktemp -d)
mkdir -m 700 -p "$TEMP_DIR/import"
cp /persistent/prov_selfupdate/* "$TEMP_DIR/import"
echo -e "[storage]\\npath = \"$TEMP_DIR\"\\n[import]\\nbase_path = \"$TEMP_DIR/import\"" > "$TEMP_DIR/conf.toml"
aktualizr -c /persistent/selfupdate.toml -c "$TEMP_DIR/conf.toml" --running-mode=once

# check that the version was updated
akt_version=$(aktualizr --version)
(grep 2.0-selfupdate <<< "$akt_version") || (echo "$akt_version"; false)

# check that aktualizr-info succeeds
aktualizr-info

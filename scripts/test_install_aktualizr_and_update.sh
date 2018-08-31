#! /bin/bash

set -exuo pipefail

/persistent/selfupdate_server.py 8000&

dpkg-deb -I /persistent/aktualizr*.deb && dpkg -i /persistent/aktualizr*.deb
akt_version=$(aktualizr --version)
(grep "$(cat /persistent/aktualizr-version)" <<< "$akt_version") || (echo "$akt_version"; false)

mkdir -m 700 -p /tmp/aktualizr-storage
aktualizr -c /persistent/selfupdate.toml --running-mode=once

# check that the version was updated
akt_version=$(aktualizr --version)
(grep 2.0-selfupdate <<< "$akt_version") || (echo "$akt_version"; false)

# check that aktualizr-info succeeds
aktualizr-info

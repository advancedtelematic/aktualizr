#! /bin/bash

set -exuo pipefail

/persistent/selfupdate_server.py 8000&

dpkg-deb -I /persistent/aktualizr.deb && dpkg -i /persistent/aktualizr.deb
{
aktualizr --version | grep "$(cat /persistent/aktualizr-version)"
} && aktualizr-info --allow-migrate --loglevel 0

mkdir -m 700 -p /tmp/aktualizr-storage
aktualizr -c /persistent/selfupdate.toml --running-mode=once

aktualizr --version | grep 2.0-selfupdate

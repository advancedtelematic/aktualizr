#! /bin/bash
set -ex

clean() {
    rm -rf /persistent/prov_selfupdate
}
trap clean EXIT

/persistent/selfupdate_server.py 8000&

dpkg-deb -I /persistent/aktualizr.deb && dpkg -i /persistent/aktualizr.deb
aktualizr --version | grep "$(cat /persistent/aktualizr-version)" && aktualizr-info

aktualizr -c /persistent/selfupdate.toml --poll-once

aktualizr --version | grep 2.0-selfupdate

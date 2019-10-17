#!/bin/bash

set -exuo pipefail

# configure test.sh
export GITREPO_ROOT=${1:-$(readlink -f "$(dirname "$0")/..")}
export TEST_INSTALL_DESTDIR=${TEST_INSTALL_DESTDIR:-/persistent}
export TEST_BUILD_DIR=${TEST_BUILD_DIR:-build-ubuntu}
export TEST_CMAKE_BUILD_TYPE=Release
export TEST_WITH_INSTALL_DEB_PACKAGES=1
export TEST_WITH_OSTREE=0
export TEST_WITH_TESTSUITE=0

# build and copy aktualizr.deb and garage_deploy.deb to $TEST_INSTALL_DESTDIR
mkdir -p "$TEST_INSTALL_DESTDIR"
# note: executables are stripped, following common conventions in .deb packages
LDFLAGS="-s" "$GITREPO_ROOT/scripts/test.sh"

git -C "$GITREPO_ROOT" fetch --tags --unshallow || true
"$GITREPO_ROOT/scripts/get_version.sh" git "$GITREPO_ROOT" > "$TEST_INSTALL_DESTDIR/aktualizr-version"

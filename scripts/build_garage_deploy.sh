#!/bin/bash

set -exuo pipefail

# configure test.sh
export GITREPO_ROOT=${1:-$(readlink -f "$(dirname "$0")/..")}
export TEST_INSTALL_DESTDIR=${TEST_INSTALL_DESTDIR:-/persistent}
export TEST_BUILD_DIR=build-debstable
export TEST_CMAKE_BUILD_TYPE=Release
export TEST_WITH_INSTALL_DEB_PACKAGES=1
export TEST_WITH_TESTSUITE=0

# build and copy aktualizr.deb to $TEST_INSTALL_DESTDIR
"${GITREPO_ROOT}/scripts/test.sh"

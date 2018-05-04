#!/usr/bin/env bash

set -exuo pipefail

PKG_SRCDIR="${1:-/persistent}"
echo "Building docker for testing aktualizr deb package inside it."
docker build -t advancedtelematic/aktualizr_ubuntu_test -f Dockerfile.test-install.xenial .
echo "Running docker container with aktualizr debian package inside."
docker run -v "${PKG_SRCDIR}":/persistent -t advancedtelematic/aktualizr_ubuntu_test

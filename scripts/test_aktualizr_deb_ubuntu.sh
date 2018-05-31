#!/usr/bin/env bash

set -exuo pipefail

PKG_SRCDIR="${1:-/persistent}"
IMG_TAG=aktualizr-deb-$(cat /proc/sys/kernel/random/uuid)
echo "Building docker for testing aktualizr deb package inside it."
docker build -t "${IMG_TAG}" -f Dockerfile.test-install.xenial .
echo "Running docker container with aktualizr debian package inside."
docker run --rm -v "${PKG_SRCDIR}":/persistent -t "${IMG_TAG}" /scripts/test_install_aktualizr_and_update.sh

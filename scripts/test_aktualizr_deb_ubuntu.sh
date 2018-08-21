#!/usr/bin/env bash

set -exuo pipefail

PKG_SRCDIR="${1:-/persistent}"
IMG_TAG=aktualizr-deb-$(cat /proc/sys/kernel/random/uuid)
echo "Building docker for testing aktualizr deb package inside it."
docker build -t "${IMG_TAG}" -f docker/Dockerfile-test-install.ubuntu.xenial .
echo "Running docker container with aktualizr debian package inside."
docker run --rm -v "${PKG_SRCDIR}":/persistent -t "${IMG_TAG}" /bin/bash -c "/scripts/test_install_aktualizr_and_update.sh && /scripts/test_aktualizr_repo.sh"

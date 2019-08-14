#!/bin/sh

set -exu

PKG_SRCDIR="${1:-/persistent}"
INSTALL_DOCKERFILE="${2:-Dockerfile}"
IMG_TAG=aktualizr-deb-$(cat /proc/sys/kernel/random/uuid)

echo "Building docker for testing aktualizr deb package inside it."
docker build -t "${IMG_TAG}" -f "${INSTALL_DOCKERFILE}" .
echo "Running docker container with aktualizr debian package inside."
docker run --rm -v "${PKG_SRCDIR}":/persistent -t "${IMG_TAG}" /bin/bash -c "/scripts/test_install_aktualizr_and_update.sh && /scripts/test-uptane-generator.sh"

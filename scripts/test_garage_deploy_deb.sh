#!/bin/sh

set -exuo pipefail

PKG_SRCDIR="${1:-/persistent}"
INSTALL_DOCKERFILE="${2:-Dockerfile}"
IMG_TAG=garage-deploy-$(cat /proc/sys/kernel/random/uuid)

echo "Building docker for testing garage-deploy deb package inside it."
docker build -t "${IMG_TAG}" -f "${INSTALL_DOCKERFILE}" .
echo "Running docker container with garage-deploy debian package inside."
docker run --rm -v "${PKG_SRCDIR}":/persistent -t "${IMG_TAG}" /scripts/test_install_garage_deploy.sh

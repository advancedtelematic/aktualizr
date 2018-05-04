#!/usr/bin/env bash

set -exuo pipefail

PKG_SRCDIR="${1:-/persistent}"
echo "Building docker for testing garage-deploy deb package inside it."
docker build -t advancedtelematic/garage_deploy_test -f Dockerfile.garage-deploy.deb-stable .
echo "Running docker container with garage-deploy debian package inside."
docker run -v "${PKG_SRCDIR}":/persistent -t advancedtelematic/garage_deploy_test

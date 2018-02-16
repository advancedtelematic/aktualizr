#!/usr/bin/env bash
set -e

if [ "$1" == "Dockerfile.noostree" ]; then
  echo "Building docker for testing aktualizr deb package inside it."
  docker build -t advancedtelematic/aktualizr_ubuntu_test -f Dockerfile.test-install.xenial .
  echo "Running docker container with aktualizr debian package inside."
  docker run -v /persistent:/persistent -t advancedtelematic/aktualizr_ubuntu_test
fi

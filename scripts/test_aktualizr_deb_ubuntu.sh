#!/usr/bin/env bash
set -e

if [ "$1" == "Dockerfile.noostree" ]; then
  echo "Building docker for test deb package inside it."
  docker build -t advancedtelematic/aktualizr_ubuntu_test -f Dockerfile.test-install.xenial .
  echo "Running docker conatiner with debian package inside."
  docker run -v /persistent:/persistent -t advancedtelematic/aktualizr_ubuntu_test
fi

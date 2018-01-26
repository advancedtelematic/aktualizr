#!/usr/bin/env bash
set -e

if [ "$1" == "Dockerfile.16.04" ]; then
  echo "Building docker for test deb package inside it."
  docker build -t advancedtelematic/aktualizr_ubuntu_test -f Dockerfile.test.16.04 .
  echo "Running docker container with debian package inside."
  docker run -v /persistent:/persistent -t advancedtelematic/aktualizr_ubuntu_test
fi

#!/usr/bin/env bash
set -e

if [ "$1" == "Dockerfile.deb-stable" ]; then
  echo "Building docker for test deb package inside it."
  docker build -t advancedtelematic/garage_deploy_test -f src/sota_tools/Dockerfile.debian.stable .
  echo "Running docker conatiner with debian packege inside."
  docker run -v /persistant:/persistant -t advancedtelematic/garage_deploy_test
fi

#!/bin/sh

# build aktualizr as a docker image

REPO_ROOT="${1:-$(readlink -f "$(dirname "$0")/..")}"

set -ex

docker build -t advancedtelematic/aktualizr-base -f "$REPO_ROOT/docker/Dockerfile.ubuntu.bionic" "$REPO_ROOT"
docker build -t advancedtelematic/aktualizr-app -f "$REPO_ROOT/docker/Dockerfile.aktualizr" "$REPO_ROOT"

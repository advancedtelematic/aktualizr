#!/usr/bin/env bash

set -ex

cd ..

docker build -f docker/Dockerfile.ubuntu.xenial -t advancedtelematic/aktualizr-base .
docker build -f docker/Dockerfile.e2e -t advancedtelematic/aktualizr .
docker push advancedtelematic/aktualizr

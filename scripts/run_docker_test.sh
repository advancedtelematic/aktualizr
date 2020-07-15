#!/bin/bash

set -euo pipefail

# Utility to help running in-docker tests in the same conditions as CI (Gitlab)
#
# example:
# ./scripts/run_docker_test.sh docker/Dockerfile.ubuntu.focal \
#                             -eTEST_BUILD_DIR=build-openssl11 \
#                             -eTEST_CMAKE_BUILD_TYPE=Valgrind \
#                             -eTEST_TESTSUITE_ONLY=crypto \
#                             -eTEST_WITH_STATICTESTS=1 \
#                             -- ./scripts/test.sh
# alternatively:
# ./scripts/run_docker_test.sh docker/Dockerfile.ubuntu.focal
# # (shell starting...)
# testuser@xxx:yyy$ TEST_BUILD_DIR=build-openssl11 \
#                   TEST_CMAKE_BUILD_TYPE=Valgrind \
#                   TEST_TESTSUITE_ONLY=crypto \
#                   TEST_WITH_STATICTESTS=1 \
#                   ./scripts/test.sh


# first arg: dockerfile
DOCKERFILE=${1:-Dockerfile}
if [[ $# -ge 1 ]]; then
    shift 1
fi

# then docker options
DOCKER_OPTS=()
while [[ ($# -ge 1) && ($1 != "--") ]]; do
    DOCKER_OPTS+=("$1")
    shift 1
done

# then program to run under docker (leave blank to start a shell)
if [[ ($# -ge 1) && ($1 = "--") ]]; then
    shift 1
fi

set -x

IMG_TAG=dev-$(cat /proc/sys/kernel/random/uuid)

docker build -t "${IMG_TAG}" -f "$DOCKERFILE" .


# Prevent DOCKER_OPTS[@]: unbound variable
# From SO: https://stackoverflow.com/a/34361807/6096518
OPTS_STR=${DOCKER_OPTS[@]+"${DOCKER_OPTS[@]}"}

# run under current user, mounting current directory at the same location in the container
#
# note: we've switched back to running the tests as root on CI when we switched from Jenkins to Gitlab
# it would be great to revert to the old way at some point
docker run -u "$(id -u):$(id -g)" -v "$PWD:$PWD" -w "$PWD" --rm $OPTS_STR -it "${IMG_TAG}" "$@"

#!/bin/bash

set -euo pipefail

# Utility to help running in-docker tests in the same conditions as CI (Jenkins)
#
# example:
# ./scripts/run_docker_test.sh Dockerfile.deb-testing \
#                             -eTEST_BUILD_DIR=build-openssl11 \
#                             -eTEST_CMAKE_BUILD_TYPE=Valgrind \
#                             -eTEST_TESTSUITE_ONLY=crypto \
#                             -eTEST_WITH_STATICTESTS=1 \
#                             -- ./scripts/test.sh
# alternatively:
# ./scripts/run_docker_test.sh Dockerfile.deb-testing
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

# run under current user, mounting current directory at the same location in the container
docker run -u "$(id -u):$(id -g)" -v "$PWD:$PWD" -w "$PWD" --rm "${DOCKER_OPTS[@]}" -it "${IMG_TAG}" "$@"

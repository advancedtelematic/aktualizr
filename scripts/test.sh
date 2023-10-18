#! /bin/bash

set -euo pipefail

GITREPO_ROOT="${1:-$(readlink -f "$(dirname "$0")/..")}"

# Test options: test stages, additional checkers, compile options
TEST_BUILD_DIR=${TEST_BUILD_DIR:-build-test}
TEST_WITH_STATICTESTS=${TEST_WITH_STATICTESTS:-0}
TEST_WITH_BUILD=${TEST_WITH_BUILD:-1}
TEST_WITH_INSTALL_DEB_PACKAGES=${TEST_WITH_INSTALL_DEB_PACKAGES:-0}
TEST_WITH_TESTSUITE=${TEST_WITH_TESTSUITE:-1}
TEST_WITH_DOCS=${TEST_WITH_DOCS:-0}

TEST_WITH_COVERAGE=${TEST_WITH_COVERAGE:-0}
TEST_WITH_SOTA_TOOLS=${TEST_WITH_SOTA_TOOLS:-1}
TEST_WITH_P11=${TEST_WITH_P11:-0}
TEST_WITH_OSTREE=${TEST_WITH_OSTREE:-1}
TEST_WITH_DEB=${TEST_WITH_DEB:-1}
TEST_WITH_FAULT_INJECTION=${TEST_WITH_FAULT_INJECTION:-0}

TEST_CC=${TEST_CC:-gcc}
TEST_CMAKE_GENERATOR=${TEST_CMAKE_GENERATOR:-Ninja}
TEST_CMAKE_BUILD_TYPE=${TEST_CMAKE_BUILD_TYPE:-Valgrind}
TEST_INSTALL_DESTDIR=${TEST_INSTALL_DESTDIR:-/persistent}
TEST_INSTALL_RELEASE_NAME=${TEST_INSTALL_RELEASE_NAME:-}
TEST_SOTA_PACKED_CREDENTIALS=${TEST_SOTA_PACKED_CREDENTIALS:-}
TEST_DRYRUN=${TEST_DRYRUN:-0}
TEST_PARALLEL_LEVEL=${TEST_PARALLEL_LEVEL:-6}
TEST_TESTSUITE_ONLY=${TEST_TESTSUITE_ONLY:-}
TEST_TESTSUITE_EXCLUDE=${TEST_TESTSUITE_EXCLUDE:-}
TEST_PKCS11_MODULE_PATH=${TEST_PKCS11_MODULE_PATH:-/usr/lib/softhsm/libsofthsm2.so}
# note: on Ubuntu bionic, use /usr/lib/engines/engine_pkcs11.so on xenial
TEST_PKCS11_ENGINE_PATH=${TEST_PKCS11_ENGINE_PATH:-/usr/lib/x86_64-linux-gnu/engines-1.1/libpkcs11.so}
git config --global --add safe.directory /home/runner/work/aktualizr/aktualizr
# Build CMake arguments
CMAKE_ARGS=()
CMAKE_ARGS+=("-G$TEST_CMAKE_GENERATOR")
CMAKE_ARGS+=("-DCMAKE_BUILD_TYPE=${TEST_CMAKE_BUILD_TYPE}")
if [[ $TEST_WITH_COVERAGE = 1 ]]; then CMAKE_ARGS+=("-DBUILD_WITH_CODE_COVERAGE=ON"); fi
if [[ $TEST_WITH_SOTA_TOOLS = 1 ]]; then CMAKE_ARGS+=("-DBUILD_SOTA_TOOLS=ON"); fi
if [[ $TEST_WITH_P11 = 1 ]]; then
    CMAKE_ARGS+=("-DBUILD_P11=ON")
    CMAKE_ARGS+=("-DPKCS11_ENGINE_PATH=${TEST_PKCS11_ENGINE_PATH}")
    CMAKE_ARGS+=("-DTEST_PKCS11_MODULE_PATH=${TEST_PKCS11_MODULE_PATH}")
fi
if [[ $TEST_WITH_OSTREE = 1 ]]; then CMAKE_ARGS+=("-DBUILD_OSTREE=ON"); fi
if [[ $TEST_WITH_DEB = 1 ]]; then CMAKE_ARGS+=("-DBUILD_DEB=ON"); fi
if [[ $TEST_WITH_FAULT_INJECTION = 1 ]]; then CMAKE_ARGS+=("-DFAULT_INJECTION=ON"); fi
if [[ -n $TEST_SOTA_PACKED_CREDENTIALS ]]; then
    CMAKE_ARGS+=("-DSOTA_PACKED_CREDENTIALS=$TEST_SOTA_PACKED_CREDENTIALS");
fi
# set these in any case so that it's immune to CMake's caching (it's often
# useful to change these while using the same build directory)
CMAKE_ARGS+=("-DTESTSUITE_ONLY=${TEST_TESTSUITE_ONLY}");
CMAKE_ARGS+=("-DTESTSUITE_EXCLUDE=${TEST_TESTSUITE_EXCLUDE}")
echo ">> CMake options: ${CMAKE_ARGS[*]}"

# failure handling
FAILURES=()
collect_failures() {
    if [[ ${#FAILURES[@]} != 0 ]]; then
        echo "The following checks failed:"
        for check in "${FAILURES[@]}"; do
            echo "- $check"
        done
        exit 1
    fi
}
add_failure() {
    set +x
    FAILURES+=("$1")
}
add_fatal_failure() {
    set +x
    FAILURES+=("$1")
    collect_failures
}
run_make() (
    set +x
    local target=${1:-}
    shift 1
    if [ -n "$target" ]; then
        target=(--target "$target")
    else
        target=()
    fi
    (
        set +u  # needed for bash < 4.4
        set -x
        export CTEST_OUTPUT_ON_FAILURE=1
        export CTEST_PARALLEL_LEVEL="${TEST_PARALLEL_LEVEL}"
        cmake --build . "${target[@]}" -- -j"${TEST_PARALLEL_LEVEL}" "$@"
    )
)

# Test stages
mkdir -p "${TEST_BUILD_DIR}"
cd "${TEST_BUILD_DIR}"


if [[ $TEST_WITH_P11 = 1 ]]; then
    echo ">> Setting up P11"
    if [[ $TEST_DRYRUN != 1 ]]; then
        set -x
        export SOFTHSM2_CONF="$PWD/softhsm2.conf"
        export TOKEN_DIR="$PWD/softhsm2-tokens"
        rm -rf "${TOKEN_DIR}"
        mkdir -p "${TOKEN_DIR}"
        cp "$GITREPO_ROOT/tests/test_data/softhsm2.conf" "${SOFTHSM2_CONF}"

        "$GITREPO_ROOT/scripts/setup_hsm.sh" || add_fatal_failure "setup hsm"
        set +x
    fi
fi

echo ">> Running CMake"
if [[ $TEST_DRYRUN != 1 ]]; then
    (
    if [[ $TEST_CC = gcc ]]; then
        export CC=gcc CXX=g++
    elif [[ $TEST_CC = clang ]]; then
        export CC=clang CXX=clang++
    fi

    set -x
    cmake "${CMAKE_ARGS[@]}" "${GITREPO_ROOT}" || add_fatal_failure "cmake configure"
    )
fi

if [[ $TEST_WITH_STATICTESTS = 1 ]]; then
    echo ">> Running static checks"
    if [[ $TEST_DRYRUN != 1 ]]; then
        set -x
        run_make check-format -k 0 || add_failure "formatting"
        run_make clang-tidy -k 0 || add_failure "static checks"
        set +x
    fi
fi

if [[ $TEST_WITH_BUILD = 1 ]]; then
    echo ">> Building and installing"
    if [[ $TEST_DRYRUN != 1 ]]; then
        set -x
        run_make || add_fatal_failure "make"

        # Check that 'make install' works
        DESTDIR=/tmp/aktualizr run_make install || add_failure "make install"
        set +x
    fi
fi

if [[ $TEST_WITH_INSTALL_DEB_PACKAGES = 1 ]]; then
    echo ">> Building debian package"
    if [[ $TEST_DRYRUN != 1 ]]; then
        set -x
        run_make package || add_failure "make package"

        # install garage-deploy
        cp ./*garage_deploy.deb "${TEST_INSTALL_DESTDIR}/garage_deploy${TEST_INSTALL_RELEASE_NAME}.deb"

        # install aktualizr.deb
        cp ./*aktualizr.deb "${TEST_INSTALL_DESTDIR}/aktualizr${TEST_INSTALL_RELEASE_NAME}.deb"
        set +x
    fi
fi

if [[ $TEST_WITH_TESTSUITE = 1 ]]; then
    if [[ $TEST_WITH_COVERAGE = 1 ]]; then
        echo ">> Running test suite with coverage"
        if [[ $TEST_DRYRUN != 1 ]]; then
            set -x
            run_make coverage || add_failure "testsuite with coverage"

            if [[ -n ${CODECOV_TOKEN:-} ]]; then
                bash <(curl -s https://codecov.io/bash) -f '!*/#usr*' -f '!*/^#third_party*' -R "${GITREPO_ROOT}" -s . > /dev/null
            else
                echo "Skipping codecov.io upload"
            fi
            set +x
        fi
    else
        echo ">> Running test suite"
        if [[ $TEST_DRYRUN != 1 ]]; then
            set -x
            run_make check || add_failure "testsuite"
            set +x
        fi
    fi
fi

if [[ $TEST_WITH_DOCS = 1 ]]; then
    echo ">> Running make docs"
    if [[ $TEST_DRYRUN != 1 ]]; then
        set -x
        run_make docs || add_failure "make docs"
        set +x
   fi
fi

collect_failures

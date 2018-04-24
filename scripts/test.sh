#! /bin/bash
set -e

GITREPO_ROOT="${1:-$(readlink -f "$(dirname "$0")/..")}"

# Test options: test stages, additional checkers, compile options
TEST_BUILD_DIR=${TEST_BUILD_DIR:-build-test}
TEST_WITH_STATICTESTS=${TEST_WITH_STATICTESTS:-0}
TEST_WITH_BUILD=${TEST_WITH_BUILD:-1}
TEST_WITH_INSTALLGARAGEDEPLOY=${TEST_WITH_INSTALLGARAGEDEPLOY:-0}
TEST_WITH_TESTSUITE=${TEST_WITH_TESTSUITE:-1}

TEST_WITH_VALGRIND=${TEST_WITH_VALGRIND:-1}
TEST_WITH_COVERAGE=${TEST_WITH_COVERAGE:-0}

TEST_WITH_SOTA_TOOLS=${TEST_WITH_SOTA_TOOLS:-1}
TEST_WITH_P11=${TEST_WITH_P11:-0}
TEST_WITH_OSTREE=${TEST_WITH_OSTREE:-1}
TEST_WITH_DEB=${TEST_WITH_DEB:-1}

TEST_DRYRUN=${TEST_DRYRUN:-0}

# Build CMake arguments
CMAKE_ARGS=()
if [[ $TEST_WITH_VALGRIND = 1 ]]; then CMAKE_ARGS+=("-DCMAKE_BUILD_TYPE=Valgrind"); fi
if [[ $TEST_WITH_COVERAGE = 1 ]]; then CMAKE_ARGS+=("-DBUILD_WITH_CODE_COVERAGE=ON"); fi
if [[ $TEST_WITH_SOTA_TOOLS = 1 ]]; then CMAKE_ARGS+=("-DBUILD_SOTA_TOOLS=ON"); fi
if [[ $TEST_WITH_P11 = 1 ]]; then
    CMAKE_ARGS+=("-DBUILD_P11=ON")
    CMAKE_ARGS+=("-DTEST_PKCS11_ENGINE_PATH=/usr/lib/engines/engine_pkcs11.so")
    CMAKE_ARGS+=("-DTEST_PKCS11_MODULE_PATH=/usr/lib/softhsm/libsofthsm2.so")
fi
if [[ $TEST_WITH_OSTREE = 1 ]]; then CMAKE_ARGS+=("-DBUILD_OSTREE=ON"); fi
if [[ $TEST_WITH_DEB = 1 ]]; then CMAKE_ARGS+=("-DBUILD_DEB=ON"); fi
echo ">> CMake options: ${CMAKE_ARGS[*]}"

# Test stages
mkdir -p "${TEST_BUILD_DIR}"
cd "${TEST_BUILD_DIR}"

if [[ $TEST_WITH_P11 = 1 ]]; then
    echo ">> Setting up P11"
    export SOFTHSM2_CONF="$PWD/softhsm2.conf"
    export TOKEN_DIR="$PWD/softhsm2-tokens"
    if [[ $TEST_DRYRUN != 1 ]]; then
        mkdir -p "${TOKEN_DIR}"
        cp "$GITREPO_ROOT/tests/test_data/softhsm2.conf" "${SOFTHSM2_CONF}"

        "$GITREPO_ROOT/scripts/setup_hsm.sh"
    fi
fi

echo ">> Running CMake"
[[ $TEST_DRYRUN = 1 ]] || cmake "${CMAKE_ARGS[@]}" "${GITREPO_ROOT}"

if [[ $TEST_WITH_STATICTESTS = 1 ]]; then
    echo ">> Running static checks"
    [[ $TEST_DRYRUN = 1 ]] || make check-format -j8
    [[ $TEST_DRYRUN = 1 ]] || make clang-tidy clang-check -j8
fi

if [[ $TEST_WITH_BUILD = 1 ]]; then
    echo ">> Building and installing"
    if [[ $TEST_DRYRUN != 1 ]]; then
        make -j8

        # Check that 'make install' works
        DESTDIR=/tmp/aktualizr make install -j8
    fi
fi

if [[ $TEST_WITH_INSTALLGARAGEDEPLOY = 1 ]]; then
    echo ">> Building debian package"
    if [[ $TEST_DRYRUN != 1 ]]; then
        make package -j8
        cp garage_deploy.deb /persistent/
    fi
fi

if [[ $TEST_WITH_TESTSUITE = 1 ]]; then
    if [[ $TEST_WITH_COVERAGE = 1 ]]; then
        echo ">> Running test suite with coverage"
        if [[ $TEST_DRYRUN != 1 ]]; then
            # not fail immediately on Jenkins, it will analyze the list of tests
            CTEST_OUTPUT_ON_FAILURE=1 CTEST_PARALLEL_LEVEL=3 make -j6 coverage || [[ $JENKINS_RUN = 1 ]]

            if [[ -n $TRAVIS_COMMIT ]]; then
                bash <(curl -s https://codecov.io/bash) -R "${GITREPO_ROOT}" -s .
            else
                echo "Not inside Travis, skipping codecov.io upload"
            fi
        fi
    else
        echo ">> Running test suite"
        if [[ $TEST_DRYRUN != 1 ]]; then
            CTEST_OUTPUT_ON_FAILURE=1 CTEST_PARALLEL_LEVEL=3 make -j6 check-full || [[ $JENKINS_RUN = 1 ]]
        fi
    fi
fi

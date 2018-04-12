#! /bin/bash
set -e

GITREPO_ROOT="${1:-$(readlink -f "$(dirname "$0")/..")}"

export SOFTHSM2_CONF="$PWD/build-coverage/softhsm2.conf"
export TOKEN_DIR="$PWD/build-coverage/softhsm2-tokens"

mkdir -p build-coverage
rm -rf "${TOKEN_DIR}"
mkdir -p "${TOKEN_DIR}"
cp "$GITREPO_ROOT/tests/test_data/softhsm2.conf" "${SOFTHSM2_CONF}"

./scripts/setup_hsm.sh

(
cd build-coverage
cmake -DBUILD_OSTREE=ON -DBUILD_SOTA_TOOLS=ON -DBUILD_DEB=ON -DBUILD_P11=ON -DBUILD_WITH_CODE_COVERAGE=ON -DTEST_PKCS11_ENGINE_PATH="/usr/lib/engines/engine_pkcs11.so" -DTEST_PKCS11_MODULE_PATH="/usr/lib/softhsm/libsofthsm2.so" "${GITREPO_ROOT}"
make -j8
CTEST_OUTPUT_ON_FAILURE=1 CTEST_PARALLEL_LEVEL=3 make -j6 coverage
)

if [ -n "$TRAVIS_COMMIT" ]; then
    bash <(curl -s https://codecov.io/bash -p src -s build-coverage)
else
    echo "Not inside Travis, skipping codecov.io upload"
fi

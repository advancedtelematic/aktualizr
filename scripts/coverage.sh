#! /bin/bash
set -e

./scripts/setup_hsm.sh

mkdir -p build-coverage
cd build-coverage
cmake -DBUILD_OSTREE=ON -DBUILD_SOTA_TOOLS=ON -DBUILD_DEB=ON -DBUILD_P11=ON -DBUILD_WITH_CODE_COVERAGE=ON -DTEST_PKCS11_ENGINE_PATH="/usr/lib/engines/engine_pkcs11.so" -DTEST_PKCS11_MODULE_PATH="/usr/lib/softhsm/libsofthsm2.so" ../src
make -j8
CTEST_OUTPUT_ON_FAILURE=1 CTEST_PARALLEL_LEVEL=3 make -j6 coverage
cd ..
if [ -n "$TRAVIS_COMMIT" ]; then
  bash <(curl -s https://codecov.io/bash -p src -s build-coverage)
else
  echo "Not inside Travis, skipping codecov.io upload"
fi

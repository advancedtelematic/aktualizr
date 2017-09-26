#! /bin/bash
set -e

# Virtual token is only needed for tests. As utilities' interface may vary with version, don't create token in different environments
if [ -z "$BUILD_ONLY" ]; then
  ./src/scripts/setup_hsm.sh
fi

mkdir -p build-test
cd build-test
cmake -DBUILD_GENIVI=ON -DBUILD_OSTREE=ON -DBUILD_P11=ON -DTEST_PKCS11_ENGINE_PATH="/usr/lib/engines/engine_pkcs11.so" -DTEST_PKCS11_MODULE_PATH="/usr/lib/x86_64-linux-gnu/softhsm/libsofthsm2.so"  ../src
make check-format
make -j8
if [ -n "$BUILD_ONLY" ]; then
  echo "Skipping test run because of BUILD_ONLY environment variable"
else
  CTEST_OUTPUT_ON_FAILURE=1 CTEST_PARALLEL_LEVEL=5 make -j6 check-full
fi

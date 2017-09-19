#! /bin/bash
set -e

# Virtual token is only needed for tests. As utilities' interface may vary with version, don't create token in different environments
if [ -z "$BUILD_ONLY" ]; then
  mkdir -p /var/lib/softhsm/tokens
  softhsm2-util --init-token --slot 0 --label "Virtual token" --pin 1234 --so-pin 1234
  softhsm2-util --import ./src/tests/test_data/implicit/pkey.pem --label "pkey" --id 02 --slot 0 --pin 1234
  openssl x509 -outform der -in ./src/tests/test_data/implicit/client.pem -out ./src/tests/test_data/implicit/client.der
  pkcs11-tool --module=/usr/lib/x86_64-linux-gnu/softhsm/libsofthsm2.so --id 1 --write-object ./src/tests/test_data/implicit/client.der --type cert --login --pin 1234
fi

mkdir -p build-test
cd build-test
cmake -DBUILD_GENIVI=ON -DBUILD_OSTREE=ON -DTEST_PKCS11_ENGINE_PATH="/usr/lib/engines/engine_pkcs11.so" -DTEST_PKCS11_MODULE_PATH="/usr/lib/x86_64-linux-gnu/softhsm/libsofthsm2.so"  ../src
make check-format
make -j8
if [ -n "$BUILD_ONLY" ]; then
  echo "Skipping test run because of BUILD_ONLY environment variable"
else
  CTEST_OUTPUT_ON_FAILURE=1 CTEST_PARALLEL_LEVEL=5 make -j6 check-full
fi

#! /bin/bash
set -e

mkdir -p /var/lib/softhsm/tokens
softhsm2-util --init-token --slot 0 --label "Virtual token" --pin 1234 --so-pin 1234
softhsm2-util --import ./src/tests/test_data/implicit/pkey.pem --label "pkey" --id 02 --slot 0 --pin 1234
openssl x509 -outform der -in ./src/tests/test_data/implicit/client.pem -out ./src/tests/test_data/implicit/client.der
pkcs11-tool --module=/usr/lib/x86_64-linux-gnu/softhsm/libsofthsm2.so --id 1 --write-object ./src/tests/test_data/implicit/client.der --type cert --login --pin 1234

mkdir -p build-coverage
cd build-coverage
cmake -DBUILD_GENIVI=ON -DBUILD_OSTREE=ON -DBUILD_WITH_CODE_COVERAGE=ON -DTEST_PKCS11_ENGINE_PATH="/usr/lib/engines/engine_pkcs11.so" -DTEST_PKCS11_MODULE_PATH="/usr/lib/x86_64-linux-gnu/softhsm/libsofthsm2.so" ../src
make -j8
CTEST_OUTPUT_ON_FAILURE=1 CTEST_PARALLEL_LEVEL=5 make -j6 coverage
cd ..
if [ -n "$TRAVIS_COMMIT" ]; then
  bash <(curl -s https://codecov.io/bash -p src -s build-coverage)
else
  echo "Not inside Travis, skipping codecov.io upload"
fi

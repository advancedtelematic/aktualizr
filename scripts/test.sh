#! /bin/bash
set -e
mkdir -p build-test
cd build-test
cmake -DBUILD_GENIVI=ON -DBUILD_OSTREE=ON -DBUILD_TESTS=ON ../src
make -j8
if [ -n "$BUILD_ONLY" ]; then
  echo "Skipping test run because of BUILD_ONLY environment variable"
else
  CTEST_OUTPUT_ON_FAILURE=1 make -j1 qa
fi

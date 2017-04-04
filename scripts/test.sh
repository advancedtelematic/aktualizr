#! /bin/bash
set -e
mkdir -p build-test
cd build-test
cmake -DBUILD_GENIVI=ON -DBUILD_OSTREE=ON -DBUILD_TESTS=ON ../src
make -j8
CTEST_OUTPUT_ON_FAILURE=1 make -j1 qa
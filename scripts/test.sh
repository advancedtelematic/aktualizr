#! /bin/bash
set -e

mkdir -p build-test
cd build-test
cmake -DBUILD_OSTREE=ON -DBUILD_SOTA_TOOLS=ON -DBUILD_DEB=ON ../src

make check-format

make -j8

# Check that 'make install' works
DESTDIR=/tmp/aktualizr make install

if [ -n "$BUILD_ONLY" ]; then
  if [ -n "$BUILD_DEB" ]; then
    make package
    cp garage_deploy.deb /persistent/
  fi
  echo "Skipping test run because of BUILD_ONLY environment variable"
else
  CTEST_OUTPUT_ON_FAILURE=1 CTEST_PARALLEL_LEVEL=5 make -j6 check-full
fi

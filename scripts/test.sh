#! /bin/bash
set -e

mkdir -p build-test
cd build-test
cmake -DBUILD_GENIVI=ON -DBUILD_OSTREE=ON -DBUILD_SOTA_TOOLS=ON ../src
if [ -z $NO_FORMAT_CHECK ]; then
  make check-format
fi

make -j8
if [ -n "$BUILD_ONLY" ]; then
  if [ -n "$BUILD_DEB" ]; then
    make garage-deploy-deb
    cp garage_deploy.deb /persistent/
  fi
  echo "Skipping test run because of BUILD_ONLY environment variable"
else
  CTEST_OUTPUT_ON_FAILURE=1 CTEST_PARALLEL_LEVEL=5 make -j6 check-full
fi

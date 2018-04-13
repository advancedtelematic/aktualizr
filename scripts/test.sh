#! /bin/bash
set -e

(
mkdir -p build-test
cd build-test
cmake -DBUILD_OSTREE=ON -DBUILD_SOTA_TOOLS=ON -DBUILD_ISOTP=ON -DBUILD_DEB=ON -DCMAKE_BUILD_TYPE=Valgrind ../src

make -j8

# Check that 'make install' works
DESTDIR=/tmp/aktualizr make install -j8

if [ -n "$BUILD_ONLY" ]; then
  if [ -n "$BUILD_DEB" ]; then
    make package -j8
    cp garage_deploy.deb /persistent/
  fi
  echo "Skipping test run because of BUILD_ONLY environment variable"
else
  CTEST_OUTPUT_ON_FAILURE=1 CTEST_PARALLEL_LEVEL=2 make -j6 check-full
fi
)

(
if [ -n "$STATIC_CHECKS" ]; then
  mkdir -p build-static-checks
  cd build-static-checks
  cmake -DBUILD_OSTREE=ON -DBUILD_SOTA_TOOLS=ON -DBUILD_ISOTP=ON -DBUILD_DEB=ON -DBUILD_P11=ON ../src

  make check-format -j8
  make clang-tidy -j8
  make clang-check -j8
fi
)

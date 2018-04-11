#! /bin/bash
set -e

(
mkdir -p build-test
cd build-test
cmake -GNinja -DBUILD_OSTREE=ON -DBUILD_SOTA_TOOLS=ON -DBUILD_ISOTP=ON -DBUILD_DEB=ON -DCMAKE_BUILD_TYPE=Valgrind ../src

ninja -v

# Check that 'make install' works
DESTDIR=/tmp/aktualizr ninja -v install

if [ -n "$BUILD_ONLY" ]; then
  if [ -n "$BUILD_DEB" ]; then
    ninja -v package
    cp garage_deploy.deb /persistent/
  fi
  echo "Skipping test run because of BUILD_ONLY environment variable"
else
  CTEST_OUTPUT_ON_FAILURE=1 CTEST_PARALLEL_LEVEL=2 ninja -v check-full
fi
)

(
if [ -n "$STATIC_CHECKS" ]; then
  mkdir -p build-static-checks
  cd build-static-checks
  cmake -GNinja -DBUILD_OSTREE=ON -DBUILD_SOTA_TOOLS=ON -DBUILD_ISOTP=ON -DBUILD_DEB=ON -DBUILD_P11=ON ../src

  ninja -v check-format clang-tidy clang-check
fi
)

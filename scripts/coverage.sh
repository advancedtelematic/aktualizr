#! /bin/bash
set -e
mkdir -p build-coverage
cd build-coverage
cmake -DBUILD_GENIVI=ON -DBUILD_OSTREE=ON -DBUILD_WITH_CODE_COVERAGE=ON ../src
make -j8
CTEST_OUTPUT_ON_FAILURE=1 CTEST_PARALLEL_LEVEL=4 make -j6 coverage
cd ..
if [ -n "$TRAVIS_COMMIT" ]; then
  bash <(curl -s https://codecov.io/bash -p src -s build-coverage)
else
  echo "Not inside Travis, skipping codecov.io upload"
fi

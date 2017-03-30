#! /bin/bash
set -e
mkdir -p build-coverage
cd build-coverage
cmake -DBUILD_GENIVI=ON -DBUILD_WITH_CODE_COVERAGE=ON ../src
make -j8
CTEST_OUTPUT_ON_FAILURE=1 make -j1 coverage
cd ..
if [ -n "$CODECOV_TOKEN" ]; then
  bash <(curl -s https://codecov.io/bash)
else
  echo "No CODECOV_TOKEN, not uploading to codecov.io"
fi
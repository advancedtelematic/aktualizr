#!/bin/bash

set -e

mkdir -p build-static-checks
cd build-static-checks
cmake -DBUILD_OSTREE=ON -DBUILD_SOTA_TOOLS=ON -DBUILD_DEB=ON -DBUILD_P11=ON ../src

make check-format
make clang-tidy -j8
make clang-check -j8

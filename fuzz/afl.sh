#!/bin/bash
set -e

mkdir -p build output
rm -rf build/* output/*
cd build

cmake ../.. \
      -DCMAKE_CXX_COMPILER=afl-g++ \
      -DCMAKE_CC_COMPILER=afl-gcc \
      -DENABLE_FUZZING=On \
      -DENABLE_SANITIZERS=On \
      -DBUILD_SHARED_LIBS=Off \
      -DBUILD_OSTREE=ON

make -j8 fuzz

#! /bin/bash
set -e

mkdir -p build-ubuntu
cd build-ubuntu
cmake -DCMAKE_BUILD_TYPE=Release ../src

make -j8
make package
cp aktualizr*.deb /persistent/aktualizr.deb

git -C ../src describe > /persistent/aktualizr-version

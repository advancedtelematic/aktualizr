#! /bin/bash
set -e

mkdir -p build-ubuntu
cd build-ubuntu
cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_OSTREE=OFF -DBUILD_DEB=ON ../src

make -j8
make package
cp aktualizr*.deb /persistent/aktualizr.deb
cp -rf ../src/tests/test_data/prov_selfupdate /persistent/
cp -rf ../src/tests/config_selfupdate.toml /persistent/
cp -rf ../src/scripts/selfupdate_server.py /persistent/
cp -f ../src/scripts/test_aktualizr_deb_and_update.sh /persistent/test_aktualizr_deb_and_update.sh
cp -rf ../src/tests/test_data/fake_root /persistent/


git -C ../src describe > /persistent/aktualizr-version

#!/usr/bin/env bash
set -e

apt-get update && apt-get -y install docker liblzma-dev bison e2fslibs-dev libgpgme11-dev libglib2.0-dev gcc g++ make cmake git psmisc dbus libdbus-1-dev libjansson-dev libgtest-dev google-mock libssl-dev autoconf automake pkg-config libtool libexpat1-dev libboost-program-options-dev libboost-test-dev libboost-random-dev libboost-regex-dev libboost-dev libboost-system-dev libboost-thread-dev libboost-log-dev libjsoncpp-dev curl libcurl4-openssl-dev lcov clang clang-format-3.8
apt-get update && apt-get -y install ostree libostree-dev libarchive-dev libsodium-dev libp11-dev softhsm2 opensc libengine-pkcs11-openssl

mkdir -p build-garagre-deploy
cd build-garagre-deploy
cmake -DBUILD_OSTREE=ON -DBUILD_SOTA_TOOLS=ON ..
make garage-push-deb
docker build src/sota_tools/Dockerfile.debian.stable . -t garage_deploy
docker run --rm garage_deploy

FROM ubuntu:xenial
LABEL Description="Aktualizr testing dockerfile"

ENV DEBIAN_FRONTEND noninteractive

RUN apt-get update
RUN apt-get -y install debian-archive-keyring
RUN echo "deb http://httpredir.debian.org/debian jessie-backports main contrib non-free" > /etc/apt/sources.list.d/jessie-backports.list

# It is important to run these in the same RUN command, because otherwise
# Docker layer caching breaks us
RUN apt-get update && apt-get -y install wget liblzma-dev bison e2fslibs-dev libgpgme11-dev libglib2.0-dev gcc g++ make cmake git psmisc dbus python3-dbus python3-gi python3-openssl libdbus-1-dev libjansson-dev libgtest-dev libssl-dev autoconf automake pkg-config libtool libexpat1-dev libboost-program-options-dev libboost-test-dev libboost-random-dev libboost-regex-dev libboost-dev libboost-system-dev libboost-date-time-dev libboost-thread-dev libboost-log-dev libjsoncpp-dev curl libcurl4-openssl-dev lcov clang clang-format-3.8
RUN apt-get update && apt-get -y install ostree libostree-dev libsodium-dev libarchive-dev python3-venv python3-dev valgrind libp11-dev softhsm2 opensc libengine-pkcs11-openssl
WORKDIR aktualizr
ADD . src

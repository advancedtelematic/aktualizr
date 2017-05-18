FROM ubuntu:xenial
LABEL Description="Aktualizr testing dockerfile"

ENV DEBIAN_FRONTEND noninteractive

RUN apt-get update
RUN apt-get -y install debian-archive-keyring
RUN echo "deb http://httpredir.debian.org/debian jessie-backports main contrib non-free" > /etc/apt/sources.list.d/jessie-backports.list

RUN apt-get update
RUN apt-get -y install liblzma-dev bison e2fslibs-dev libgpgme11-dev libglib2.0-dev gcc g++ make cmake git psmisc dbus python-dbus python-gobject-2 libdbus-1-dev libjansson-dev libgtest-dev libssl-dev autoconf automake pkg-config libtool libexpat1-dev libboost-program-options-dev libboost-test-dev libboost-regex-dev libboost-dev libboost-system-dev libboost-thread-dev libboost-log-dev libjsoncpp-dev curl libcurl4-openssl-dev lcov clang clang-format-3.8
RUN apt-get -y install ostree libostree-dev libsodium-dev python3-flask
WORKDIR aktualizr
ADD . src
FROM ubuntu:xenial
LABEL Description="Aktualizr testing dockerfile for Ubuntu Xenial with p11"

ENV DEBIAN_FRONTEND noninteractive

RUN apt-get update && apt-get -y install debian-archive-keyring
RUN echo "deb http://httpredir.debian.org/debian jessie-backports main contrib non-free" > /etc/apt/sources.list.d/jessie-backports.list

# It is important to run these in the same RUN command, because otherwise
# Docker layer caching breaks us
RUN apt-get update && apt-get -y install \
  apt-transport-https \
  autoconf \
  asn1c \
  automake \
  bison \
  cmake \
  curl \
  e2fslibs-dev \
  g++ \
  gcc \
  git \
  lcov \
  libarchive-dev \
  libboost-dev \
  libboost-log-dev \
  libboost-program-options-dev \
  libboost-random-dev \
  libboost-regex-dev \
  libboost-system-dev \
  libboost-test-dev \
  libboost-thread-dev \
  libcurl4-openssl-dev \
  libdpkg-dev \
  libengine-pkcs11-openssl \
  libexpat1-dev \
  libffi-dev \
  libglib2.0-dev \
  libgpgme11-dev \
  libgtest-dev \
  liblzma-dev \
  libostree-dev \
  libsodium-dev \
  libsqlite3-dev \
  libssl-dev \
  libsystemd-dev \
  libtool \
  make \
  opensc \
  ostree \
  pkg-config \
  psmisc \
  python3-dev \
  python3-gi \
  python3-openssl \
  python3-pip \
  python3-requests \
  python3-venv \
  valgrind \
  wget

RUN echo "deb http://mirrors.kernel.org/ubuntu/ artful main multiverse restricted universe" > /etc/apt/sources.list.d/artful.list
RUN apt-get update && apt-get -y install \
  libp11-2 \
  libp11-dev \
  softhsm2

WORKDIR aktualizr
ADD . src

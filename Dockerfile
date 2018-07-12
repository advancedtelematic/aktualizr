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
  libboost-iostreams-dev \
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
  libfuse-dev \
  libglib2.0-dev \
  libgpgme11-dev \
  libgtest-dev \
  liblzma-dev \
  libsodium-dev \
  libsqlite3-dev \
  libssl-dev \
  libsystemd-dev \
  libtool \
  make \
  opensc \
  pkg-config \
  psmisc \
  python3-dev \
  python3-gi \
  python3-openssl \
  python3-pip \
  python3-venv \
  sqlite3 \
  wget \
  zip

# Includes workaround for this bug:
# https://bugs.launchpad.net/ubuntu/+source/valgrind/+bug/1501545
# https://stackoverflow.com/questions/37032339/valgrind-unrecognised-instruction
RUN echo "deb http://mirrors.kernel.org/ubuntu/ artful main multiverse restricted universe" > /etc/apt/sources.list.d/artful.list
RUN apt-get update && apt-get -y install \
  libp11-2 \
  libp11-dev \
  softhsm2 \
  valgrind

RUN mkdir ostree
WORKDIR ostree
RUN git init && git remote add origin https://github.com/ostreedev/ostree
RUN git fetch origin v2018.1 && git checkout FETCH_HEAD
RUN NOCONFIGURE=1 ./autogen.sh
RUN ./configure CFLAGS='-Wno-error=missing-prototypes' --with-libarchive --disable-gtk-doc --disable-gtk-doc-html --disable-gtk-doc-pdf --disable-man --with-builtin-grub2-mkconfig --with-curl --without-soup --prefix=/usr
RUN make VERBOSE=1 -j4
RUN make install

RUN useradd testuser

WORKDIR aktualizr
ADD . src

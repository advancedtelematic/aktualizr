FROM ubuntu:bionic
LABEL Description="Aktualizr testing dockerfile for Ubuntu Bionic with p11"

ENV DEBIAN_FRONTEND noninteractive

# It is important to run these in the same RUN command, because otherwise
# Docker layer caching breaks us
RUN apt-get update && apt-get -y install \
  apt-transport-https \
  autoconf \
  asn1c \
  automake \
  bison \
  clang-format-6.0 \
  cmake \
  curl \
  e2fslibs-dev \
  fiu-utils \
  g++ \
  gcc \
  git \
  lcov \
  libarchive-dev \
  libboost-dev \
  libboost-log-dev \
  libboost-program-options-dev \
  libboost-system-dev \
  libboost-test-dev \
  libboost-thread-dev \
  libcurl4-openssl-dev \
  libdpkg-dev \
  libengine-pkcs11-openssl \
  libexpat1-dev \
  libffi-dev \
  libfiu-dev \
  libfuse-dev \
  libglib2.0-dev \
  libgpgme11-dev \
  libgtest-dev \
  liblzma-dev \
  libp11-3 \
  libp11-dev \
  libsodium-dev \
  libsqlite3-dev \
  libssl-dev \
  libsystemd-dev \
  libtool \
  lshw \
  make \
  opensc \
  net-tools \
  pkg-config \
  psmisc \
  python3-dev \
  python3-gi \
  python3-openssl \
  python3-pip \
  python3-venv \
  softhsm2 \
  sqlite3 \
  valgrind \
  wget \
  zip

WORKDIR /ostree
RUN git init && git remote add origin https://github.com/ostreedev/ostree
RUN git fetch origin v2018.9 && git checkout FETCH_HEAD
RUN NOCONFIGURE=1 ./autogen.sh
RUN ./configure CFLAGS='-Wno-error=missing-prototypes' --with-libarchive --disable-gtk-doc --disable-gtk-doc-html --disable-gtk-doc-pdf --disable-man --with-builtin-grub2-mkconfig --with-curl --without-soup --prefix=/usr
RUN make VERBOSE=1 -j4
RUN make install

RUN pip3 install awscli

RUN useradd testuser

WORKDIR /

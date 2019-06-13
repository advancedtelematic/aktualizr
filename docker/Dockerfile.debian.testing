FROM debian:testing
LABEL Description="Aktualizr testing dockerfile for Debian Unstable + static checks"

ENV DEBIAN_FRONTEND noninteractive

# It is important to run these in the same RUN command, because otherwise
# Docker layer caching breaks us
RUN apt-get update && apt-get -y install \
  apt-transport-https \
  asn1c \
  autoconf \
  automake \
  bison \
  clang-6.0 \
  clang-tidy-6.0 \
  clang-tools-6.0 \
  clang-format-6.0 \
  cmake \
  curl \
  doxygen \
  dpkg-dev \
  e2fslibs-dev \
  g++ \
  gcc \
  git \
  graphviz \
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
  libfuse-dev \
  libglib2.0-dev \
  libgpgme11-dev \
  libgtest-dev \
  liblzma-dev \
  libp11-dev \
  libsodium-dev \
  libsqlite3-dev \
  libssl-dev \
  libsystemd-dev \
  libtool \
  lshw \
  make \
  net-tools \
  opensc \
  pkg-config \
  psmisc \
  python3-dev \
  python3-gi \
  python3-openssl \
  python3-venv \
  softhsm2 \
  sqlite3 \
  valgrind \
  wget

RUN ln -s clang-6.0 /usr/bin/clang && \
    ln -s clang++-6.0 /usr/bin/clang++

WORKDIR /ostree
RUN git init && git remote add origin https://github.com/ostreedev/ostree
RUN git fetch origin v2018.9 && git checkout FETCH_HEAD
RUN NOCONFIGURE=1 ./autogen.sh
RUN ./configure CFLAGS='-Wno-error=missing-prototypes' --with-libarchive --disable-gtk-doc --disable-gtk-doc-html --disable-gtk-doc-pdf --disable-man --with-builtin-grub2-mkconfig --with-curl --without-soup --prefix=/usr
RUN make VERBOSE=1 -j4
RUN make install

RUN useradd testuser

WORKDIR /

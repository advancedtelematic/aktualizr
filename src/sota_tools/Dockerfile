FROM debian:stable
LABEL Description="sota-tools testing dockerfile"

MAINTAINER Phil Wise <phil@advancedtelematic.com>

# It is important to run these in the same RUN command, because otherwise
# Docker layer caching breaks us
RUN apt-get update -q && apt-get -qy install \
  build-essential \
  clang \
  clang-format-3.8 \
  cmake \
  g++ \
  libarchive-dev \
  libboost-dev \
  libboost-filesystem-dev \
  libboost-log-dev \
  libboost-program-options-dev \
  libboost-random-dev \
  libboost-regex-dev \
  libboost-system-dev \
  libcurl4-gnutls-dev \
  libglib2.0-dev \
  libsodium-dev \
  libssl-dev \
  make \
  && rm -rf /var/lib/apt/lists/*

WORKDIR sota-tools
COPY . src
WORKDIR build-test
RUN cmake -DBUILD_GENIVI=OFF -DBUILD_OSTREE=OFF -DBUILD_SOTA_TOOLS=ON ../src
RUN make garage-push -j8

ENTRYPOINT ["./garage-push"]

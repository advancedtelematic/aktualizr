FROM debian:stable

MAINTAINER Phil Wise <phil@advancedtelematic.com>

RUN apt-get update -q && apt-get -qy install \
    build-essential \
    cmake \
    g++ \
    libboost-dev \
    libboost-filesystem-dev \
    libboost-program-options-dev \
    libboost-system-dev \
    libcurl4-gnutls-dev \
    libglib2.0-dev \
    ninja-build \
  && rm -rf /var/lib/apt/lists/*

COPY . /src/
WORKDIR /src/
RUN cmake .
RUN make

ENTRYPOINT ["./garage-push"]

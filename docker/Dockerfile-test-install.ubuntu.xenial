FROM ubuntu:xenial
LABEL Description="Ubuntu Xenial package testing dockerfile"

ENV DEBIAN_FRONTEND noninteractive

RUN apt-get update && apt-get -y install debian-archive-keyring
RUN apt-get update && apt-get install -y \
  libarchive13 \
  libboost-log1.58.0 \
  libboost-program-options1.58.0 \
  libboost-system1.58.0 \
  libboost-test1.58.0 \
  libboost-thread1.58.0 \
  libc6 \
  libcurl3 \
  libglib2.0-0 \
  libsodium18 \
  libsqlite3-0 \
  libssl1.0.0 \
  libstdc++6 \
  lshw \
  openjdk-8-jre \
  python3 \
  zip

ADD ./scripts /scripts

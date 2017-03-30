FROM ubuntu:xenial
LABEL Description="Aktualizr testing dockerfile"

ENV DEBIAN_FRONTEND noninteractive
RUN apt-get update
RUN apt-get -y install gcc g++ make cmake git psmisc dbus python-dbus python-gtk2 libdbus-1-dev
RUN apt-get -y install libjansson-dev libgtest-dev google-mock libssl-dev autoconf automake pkg-config libtool libexpat1-dev libboost-program-options-dev libboost-test-dev libboost-regex-dev libboost-dev libboost-system-dev libboost-thread-dev libboost-log-dev libjsoncpp-dev curl libcurl4-openssl-dev
RUN apt-get -y install lcov clang clang-format-3.8 

WORKDIR aktualizr
ADD . src
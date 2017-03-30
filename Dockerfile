FROM ubuntu:xenial

ENV HOME /source
ENV DEBIAN_FRONTEND noninteractive
RUN mkdir $HOME
RUN apt-get update
RUN apt-get -y install liblzma-dev bison e2fslibs-dev libgpgme11-dev libglib2.0-dev
RUN apt-get -y install gcc g++ make cmake git psmisc dbus python-dbus python-gtk2 libdbus-1-dev
RUN apt-get -y install libjansson-dev libgtest-dev google-mock libssl-dev autoconf automake pkg-config libtool libexpat1-dev libboost-program-options-dev libboost-test-dev libboost-regex-dev libboost-dev libboost-system-dev libboost-thread-dev libboost-log-dev libjsoncpp-dev curl libcurl4-openssl-dev
RUN apt-get -y install lcov clang clang-format-3.8 

WORKDIR $HOME
COPY src/ $HOME/src/
COPY distribution $HOME/distribution/
COPY tests/ $HOME/tests/
COPY third_party/ $HOME/third_party/
COPY config/ $HOME/config/
COPY cmake-modules/ $HOME/cmake-modules/
COPY CMakeLists.txt $HOME/
RUN mkdir $HOME/build

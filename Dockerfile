FROM ubuntu:trusty

ENV HOME /source
ENV DEBIAN_FRONTEND noninteractive
RUN mkdir $HOME
RUN apt-get update
RUN apt-get -y install gcc g++ make cmake 
RUN apt-get -y install libboost-program-options-dev libboost-dev libboost-system-dev libboost-thread-dev libboost-log-dev libyaml-cpp-dev
RUN apt-get -y install lcov clang clang-format-3.8 

WORKDIR $HOME
COPY src/ $HOME/src/
COPY inc/ $HOME/inc/
COPY config/ $HOME/config/
COPY cmake-modules/ $HOME/cmake-modules/
COPY CMakeLists.txt $HOME/
RUN mkdir $HOME/build

FROM ubuntu:trusty

ENV HOME /source
ENV DEBIAN_FRONTEND noninteractive
RUN mkdir $HOME
RUN apt-get update
RUN apt-get -y install g++ make cmake 
RUN apt-get -y install libboost-program-options-dev libboost-dev libboost-system-dev libboost-thread-dev libboost-log-dev libyaml-cpp-dev

WORKDIR $HOME
COPY src/ $HOME/src/
COPY inc/ $HOME/inc/
COPY config/ $HOME/config/
COPY CMakeLists.txt $HOME/
COPY Makefile $HOME/ 
RUN mkdir $HOME/build

# Dockerfile to ensure the dependencies list is correct

FROM debian:stable

MAINTAINER Phil Wise <phil@advancedtelematic.com>

RUN apt-get update && apt-get -y dist-upgrade

RUN apt-get -y install build-essential cmake g++ libboost-dev libboost-program-options-dev libboost-filesystem-dev libboost-system-dev libcurl4-gnutls-dev ninja-build
RUN apt-get -y install clang

# Create build user
#RUN id build >/dev/null || useradd --create-home --shell /bin/bash build
#VOLUME /build
# USER build
#WORKDIR /build
#RUN mkdir build
#RUN cd build
#RUN cmake -DCMAKE_BUILD_TYPE=Debug ..
#RUN ninja -j8 qa
#
CMD ["/bin/bash"]

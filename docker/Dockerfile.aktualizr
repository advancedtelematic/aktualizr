FROM advancedtelematic/aktualizr-base as builder
LABEL Description="Aktualizr application dockerfile"

ADD . /aktualizr
WORKDIR /aktualizr/build

RUN cmake -DFAULT_INJECTION=on -DBUILD_SOTA_TOOLS=on -DBUILD_DEB=on -DCMAKE_BUILD_TYPE=Debug ..
RUN make -j8 install

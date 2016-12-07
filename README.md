[![License: MPL 2.0](https://img.shields.io/badge/License-MPL%202.0-brightgreen.svg)](https://opensource.org/licenses/MPL-2.0)
[![TravisCI Build Status](https://travis-ci.org/advancedtelematic/sota_client_cpp.svg?branch=master)](https://travis-ci.org/advancedtelematic/sota_client_cpp)
[![CII Best Practices](https://bestpractices.coreinfrastructure.org/projects/539/badge)](https://bestpractices.coreinfrastructure.org/projects/539)

sota_client_cpp
------

This project houses the C++ reference implementation of a client for the [GENIVI SOTA](https://github.com/advancedtelematic/rvi_sota_server) project. The client is intended to be installed on devices that wish to recive OTA updates from a GENIVI-compatible OTA server.

The client is responsible for:

 - Communicating with the OTA server
 - Authenticating using locally available device and user credentials
 - Reporting current software and hardware configuration to the server
 - Checking for any available updates for the device
 - Downloaded any available updates
 - Installing the updates on the system, or notifying other services of the availability of the downloaded file
 - Receiving or generating installation reports (success or failure) for attempts to install received software
 - Submitting installation reports to the server

The client maintains the integrity and confidentiality of the OTA update in transit, communicating with the server over a TLS link. The client can run either as a system service, periodically checking for updates, or can by triggered by other system interactions (for example on user request, or on receipt of a wake-up message from the OTA server).

Dependencies
------
The following debian packages are used in the project:

 - libboost-dev
 - libboost-program-options-dev (>= 1.58.0)
 - libboost-system-dev (>= 1.58.0)
 - libboost-thread-dev (>= 1.58.0)
 - libboost-log-dev (>= 1.58.0)
 - libpthread-stubs0-dev (>=0.3)
 - libyaml-cpp-dev (>=0.5.2)
 - cmake (>= 3.5.1)

Building
------

To build the client in your local environment, first install all the prerequisites, then run:

~~~
make
~~~

Testing
-----

To run the test suite:

~~~
make test
~~~

Building / Testing with Docker
-----

A Dockerfile is provided to support building / testing the application without dependencies on your local environment. If you have a working docker client and docker server running on your machine, you can build a docker image with:

~~~
docker build -t advancedtelematic/sota_client_cpp .
~~~

and run

~~~
docker run --rm -it advancedtelematic/sota_client_cpp
~~~

to build the software or

~~~
docker run --rm -it advancedtelematic/sota_client_cpp test
~~~

to run the tests.

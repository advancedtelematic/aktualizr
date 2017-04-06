# REMOTE VEHICLE INTERACTION {#mainpage}

[![Build Status](https://travis-ci.org/tjamison/rvi_lib.svg?branch=master)](https://travis-ci.org/tjamison/rvi_lib)

**(C) 2016 Jaguar Land Rover - All rights reserved.**

All documents in this repository are licensed under the Creative
Commons Attribution 4.0 International (CC BY 4.0). Click
[here](https://creativecommons.org/licenses/by/4.0/) for details.
All code in this repository is licensed under Mozilla Public License
v2 (MPLv2). Click [here](https://www.mozilla.org/en-US/MPL/2.0/) for
details.

Remote Vehicle Interaction (RVI) provides an architecture which, through its
specified components, enables connected vehicles and other devices to form a
secured distributed, sparsely connected peer-to-peer network. In particular,
local services can be registered to enable remote invocation only when the peer
presents appropriate credentials. In general, RVI provides an intermediary
between less trusted remote sources and more trusted internal sources, limiting
information that crosses trust boundaries.

`rvi_lib` provides a client implementation in C. This is supplemental to
[RVI Core](https://github.com/GENIVI/rvi_core), which adds a variety of
capabilities including server behaviors.

# Documentation
[GitHub pages](http://genivi.github.io/rvi_lib/files.html)

# Standards Used

1. [JSON](http://www.json.org/)
2. [base64url](https://en.wikipedia.org/wiki/Base64)
3. [JSON Web Token (JWT)](https://tools.ietf.org/html/draft-ietf-oauth-json-web-token-32)
4. [X.509 Certificates](https://en.wikipedia.org/wiki/X.509)
5. [Transport Layer Security (TLS)](https://tools.ietf.org/html/rfc5246)

# Build Requirements

`rvi_lib` depends on the following libraries and tools:

1. [C Standard Library](https://www-s.acm.illinois.edu/webmonkeys/book/c_guide/index.html)
2. [OpenSSL](https://www.openssl.org/)
3. [Jansson](http://www.digip.org/jansson/)
4. [LibJWT](https://github.com/benmcollins/libjwt) †
5. [GNU Autotools](https://www.gnu.org/software/autoconf/autoconf.html)

† Included as a submodule.

`rvi_lib` does not currently depend on the following but may add any or all to
support interoperability with [RVI Core](https://github.com/GENIVI/rvi_core)
nodes:

5. [mpack](http://ludocode.github.io/mpack/)

# Build Instructions

## Native Build

#### Install prerequisites
Install openssl and Jansson. On Ubuntu:
    
    $ sudo apt-get install libssl-dev libjansson-dev

Ensure the following packages are also installed:

* git
* make
* GNU Autotools
* gcc (or another c compiler)

These can also be installed via `apt` on Ubuntu:

    $ sudo apt-get install git make autoconf gcc

#### Clone repo recursively
Clone or download this repo:

    $ git clone --recursive https://github.com/GENIVI/rvi_lib.git

#### Build

Build the library using autotools. After cloning:

    $ cd rvi_lib
    $ autoreconf -i && ./configure && make

#### Install
To install the library and headers for use in applications or development, run
the following:

    $ make install

This will install both `libjwt` and `librvi` on your development platform. By
default, both are installed to `/usr/local`; use `prefix` to specify an
alternate location, as in:

    $ make install prefix=/usr 

Some operating systems will require `sudo` for either operation.

## Cross-Platform Build

Cross-platform building is supported with the Yocto Project. The
[meta-rvi](https://www.github.com/GENIVI/meta-rvi) layer provides recipes for
`librvi` and `libjwt`.

Cross-platform building is also supported by autotools. To do so, ensure you
have the appropriate cross-compiling SDK installed (and environment variables
sourced), and supply the appropriate `--host` variable to the `./configure`
script. For example, to cross-compile for [QNX660 on
ARMv7](http://www.qnx.com/support/knowledgebase.html?id=50114000001CRkJ),
modify the **Build** step to:

    $ autoreconf -i && ./configure --host=arm-unknown-nto-qnx6.6.0eabi && make

## Compile-Time Options

The typical compile-time options for a autotools-based project apply.
Additional compile-time options are specific to the RVI library.

### Build a debug version

Building a debug version of the library, appropriate for debugging via gdb or
similar, is made possible by supplying appropriate flags to the `./configure`
script:

    $ ./configure CFLAGS='-g -O0' CXXFLAGS='-g -O0'

Follow the remainder of the build process as described above.

### Install to a specific location

Once again, this is specified by supplying the appropriate flag to the
`./configure` script:

    $ ./configure --prefix=/path/to/lib

Follow the remainder of the build process as described above.

### Specify a maximum incoming message size

Define the preprocessor variable `RVI_MAX_MSG_SIZE` by supplying an argument to
the `./configure` script:

    $ ./configure CFLAGS='-D RVI_MAX_MSG_SIZE=123456' CXXFLAGS='-D RVI_MAX_MSG_SIZE=123456'

Defining this variable does the following:

1. Sets the `max_msg_size` parameter in the `au` negotiation with the remote
   RVI node to the specified value.
2. Prior to reading new data over a network socket, checks whether the buffer
   size (including previously-read data if applicable) would exceed the maximum
size
3. If the maximum size would be exceeded, destroys the already-read data, frees
   the associated memory and causes `rviProcessInput` to return `RVI_ERR_JSON`.
4. Note that if the remote RVI node sends data that is larger than the
   negotiated value, `rviProcessInput` will return `RVI_ERR_JSON` repeatedly as
the incoming data is read and discarded (up to the boundary of a new RVI
command frame).

Setting the maximum message size to a value smaller than the maximum TLS frame
size (2^14 bytes) results in undefined behavior.

#### Quick Start
If you would like to see an example of an application using `rvi_lib`, run the
following:

    $ make examples
    $ cd examples
    $ ./interactive

Initiating the RVI context requires a configuration file, along with X.509
certificates and one or more JWT credentials. See the "Configuration" section
for details on configuring your own, or use the "conf.json" file for the
pre-supplied (*insecure*) configuration.

Connect to a remote test RVI node at 38.129.64.41, port 9007. You can register
services, invoke remote services, and process incoming messages.

Insecure configuration details, including private keys and certificates, are
provided in the [examples](examples) folder.
_*THESE CREDENTIALS MUST NOT BE USED IN PRODUCTION*_.

# Configuration
When used in a calling application (such as the example "interactive" program),
`rvi_lib` requires the client to be configured with a valid X.509 certificate
for TLS exchanges, as well as (at least one) JWT-encoded RVI credential that
matches the client's X.509 certificate.

All configuration options are specified via a JSON configuration file that is
supplied when initializing the RVI library. A sample configuration file
follows:

```
    {
      "dev": {
        "key":  "./priv/clientkey.pem",
        "cert": "./priv/clientcert.pem",
        "id": "genivi.org/client/bbfbb478-d628-480a-8528-cff40d73678f"
      },
      "ca": {
        "cert": "./priv/cacert.pem",
        "dir":  "./priv/"
      },
      "creddir": "priv/"
    }
```

Note the three-part structure of the ID: domain, followed by node type,
followed by UUID. The UUID should be generated by an external method, such as
Linux's uuidgen(1) or Microsoft's guidgen.exe.

Relative or absolute paths are permitted in the configuration file. All
relative paths will be resolved relative to the working directory from which
the process is invoked.

## Generate certificates and credentials (Development)
The RVI architecture relies on X.509 certificates and JWT credentials created
out-of-band of RVI message exchanges. These JWTs must be encoded using the
RS256 algorithm (RSASSA-PKCS1-v1_5 with the SHA-256 hash algorithm).

X.509 certificates may be generated in the usual way. For development purposes,
this can be done with a local Certification Authority (CA) to issue the
certificates required. The `openssl` command-line suite must be available.

See Ivan Ristić's [OpenSSL Cookbook](https://www.feistyduck.com/library/openssl-cookbook/)
for more information about using the OpenSSL command-line suite. For
convenience, condensed steps follow.

### Create the CA
First, set up an environment for the CA to keep tabs on the certificates
issued. On a *nix system:

    $ mkdir rvica
    $ cd rvica
    $ mkdir certs private
    $ chmod g-rwx,o-rwx private
    $ echo '01' > serial
    $ touch index.txt

Then, add an openssl configuration file to the folder. A typical openssl
installation places a configuration file at `/etc/ssl/openssl.cnf`. You may use
this configuration file or copy it to the `rvica` directory and modify defaults as you wish.

Next, create a certificate signing request (CSR) and private key for the CA:

    $ openssl req -newkey rsa:1024 -sha256 -keyout private/cakey.pem \ 
    >  -out careq.pem

Sign the request. (Note that this will be a self-signed certificate, which is
typical for root CAs. No other certificates are self-signed in the RVI
architecture.)

    $ openssl x509 -req -in careq.pem -sha256 -extfile myopenssl.cnf \
    >  -extensions v3_ca -signkey private/cakey.pem -out cacert.pem

### On device: Generate private key and CSR
The device should normally generate its own private key and CSR. If the device
does not have this capacity, the key and CSR can be generated on another device
and transfered via some secure method. (See, e.g., ["Keep your SSL Private Keys
Private"](https://www.wolfssl.com/wolfSSL/Blog/Entries/2010/12/22_Keep_your_SSL_Private_Keys_Private.html)
for tips about private keys on embedded devices.) 

Generate a key and CSR just as we did for the CA:

    $ openssl req -newkey rsa:1024 -sha256 -keyout device_key.pem \
    >  -out devicereq.pem

The CSR, which does not contain any secret information, can then be sent to the
CA. Copy it to the `rvica` directory if the CSR was generated on the same
machine as the CA, or use e.g., `scp` or `sftp` to copy the file if it was
generated on a networked device.

### CA: sign CSR and generate JWT credential
Once the device's CSR is available to the CA, sign the request:

    $ openssl x509 -req -in devicereq.pem -sha256 -extfile myopenssl.cnf \
    >  -extensions usr_cert -CA cacert.pem -CAkey private\cakey.pem \
    >  -CAcreateserial -out certs/devicecert.pem

In addition, generate a JWT using the `rvi_create_credential.py` script from
[rvi_core/scripts](https://github.com/GENIVI/rvi_core/tree/develop/scripts):

    $ ./rvi_create_credential.py \
    >  --id=<id> \                          # System-wide unique credential ID
    >  --invoke='<services>' \              # Granted right(s) to invoke
    >  --receive='<services>' \             # Granted right(s) to receive
    >  --root_key=private/cakey.pem \       # The CA's private key to sign the JWT
    >  --device_cert=certs/devicecert.pem \ # The device's X.509 certificate
    >  --start='<YYYY-MM-DD HH:MM:SS>' \    # Not valid before. Default: now
    >  --stop='<YYYY-MM-DD HH:MM:SS>' \     # Valid until. Default: a year from now
    >  --jwt_out=certs/device.jwt \         # Filename to store JWT. Default: stdout
    >  --cred_out=<file> \                  # Filename to store raw JSON. Default: none
    >  --issuer="Develop" \                 # Name of issuer

The credential ID is _not_ the same as the device ID.

The following are mandatory:

* `invoke`
* `receive`
* `root_key`
* `device_cert`
* `issuer`

Please note that order of flags matters.

RVI uses a pattern-based rights management system. All remote calls are made by
passing a fully-qualified service name, which contain: domain, node type, UUID,
and additional service name information. Each item is delimited by a forward
slash ('/'). For example:

    genivi.org/vehicle/dc23f560-8635-4c10-8aeb-34c13dad60b6/control/unlock

Pattern matches consist of prefix matching, with single-level wildcards
permitted using the '+' character. In this example, the following rights
_would_ allow the device to invoke the above service if supplied to the
`--invoke` flag:

    genivi.org
    genivi.org/vehicle/+/control

The following rights would _not_ allow the device to invoke the above service:

    jaguarlandrover.com
    genivi.org/vehicle/unlock

It is up to the CA to establish a policy governing which rights are granted.

Once the CA has signed both the certificate and the RVI credential, both files
(`certs/devicecert.pem` and `certs/device.jwt`) should be returned to the
device. The device must also have the CA's certificate to add to the device's
trusted certificate store.

### Connect RVI nodes
To get full use of `rvi_lib`, you must connect to a node running [RVI
Core](https://github.com/GENIVI/rvi_core/). You will need to supply the server
node's IP address and port to the client. Additionally, the server and any
clients must present certificates and credentials signed by mutually trusted
CAs. For a typical development configuration, repeat the "Generate a device key
and CSR" and "Sign the CSR" steps above for the server. See [RVI
Core](https://github.com/GENIVI/rvi_core/) for additional configuration
details. The `tlsj_backend` configuration is recommended.

## Generate certificates and credentials (Production)

See [RVI Core](https://github.com/GENIVI/rvi_core/) for discussion of
generating certificates and RVI credentials for use in a production
environment. Note that keys should be 2048 bits or larger for deployment, as
1024-bit keys may be vulnerable to brute force attacks within the key's
lifetime.

# Contributions
`rvi_lib` is an open source project maintained by
[GENIVI](https://www.genivi.org/) for general use. Contributions are welcome,
subject to licensing compatibility with MPL v2.0. Submit issues at the the
[GENIVI Project JIRA](https://at.projects.genivi.org/jira/projects/RCC).
Submit pull requests via GitHub at the authoritative
[GENIVI/rvi_lib](https://github.com/GENIVI/rvi_lib/) repo.

# Limitations

* Thread-safety. `rvi_lib` is designed for a single-threaded environment and is
  not thread safe in its current form.
* Server capabilities. `rvi_lib` is designed to serve as a client only; it does
  not make ports available for other entities to initiate connections.
* MessagePack support. `rvi_lib` currently only supports RVI commands
  transmitted as JSON objects. [RVI Core](https://github.com/GENIVI/rvi_core)
has introduced support for MessagePack encoding, so this is a planned
enhancement.


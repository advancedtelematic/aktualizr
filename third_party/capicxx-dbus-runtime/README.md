### CommonAPI C++ D-Bus Runtime

##### Copyright
Copyright (C) 2016, Bayerische Motoren Werke Aktiengesellschaft (BMW AG).
Copyright (C) 2016, GENIVI Alliance, Inc.

This file is part of GENIVI Project IPC Common API C++.
Contributions are licensed to the GENIVI Alliance under one or more Contribution License Agreements or MPL 2.0.

##### License
This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed with this file, you can obtain one at http://mozilla.org/MPL/2.0/.

##### CommonAPI D-Bus C++ User Guide
The user guide can be found in the documentation directory of the CommonAPI-D-Bus-Tools project as AsciiDoc document. A pdf version can be found at https://github.com/GENIVI/capicxx-dbus-tools/releases.

##### Further information
https://genivi.github.io/capicxx-core-tools/

##### Build Instructions for Linux

###### Patching libdbus

CommonAPI-D-Bus needs some api functions of libdbus which are not available in actual libdbus versions. For these additional api functions it is necessary to patch the required libdbus version with all the patches in the directory src/dbus-patches. Use autotools to build libdbus.

```bash
$ wget http://dbus.freedesktop.org/releases/dbus/dbus-<VERSION>.tar.gz
$ tar -xzf dbus-<VERSION>.tar.gz
$ cd dbus-<VERSION>
$ patch -p1 < </path/to/CommonAPI-D-Bus/src/dbus-patches/patch-names>.patch 
$ ./configure --prefix=</path to your preferred installation folder for patched libdbus>
$ make -C dbus 
$ make -C dbus install
$ make install-pkgconfigDATA
```

You can change the installation directory by the prefix option or you can let it uninstalled (skip the _make install_ commands).
WARNING: Installing the patched libdbus to /usr/local can prevent your system from booting correctly at the next reboot.

###### Build CommonAPI-D-Bus Runtime

In order to build the CommonAPI-D-Bus Runtime library the pkgconfig files of the patched libdbus library must be added to the _PKG_CONFIG_PATH_.

For example, if the patched _libdbus_ library is available in /usr/local, set the _PKG_CONFIG_PATH_ variable as follows:

```bash
$ export PKG_CONFIG_PATH="/usr/local/lib/pkgconfig:$PKG_CONFIG_PATH" 
```

Now use CMake to build the CommonAPI D-Bus runtime library. We assume that your source directory is _common-api-dbus-runtime_:

```bash
$ cd common-api-dbus-runtime
$ mkdir build
$ cd build
$ cmake -D USE_INSTALLED_COMMONAPI=ON -D CMAKE_INSTALL_PREFIX=/usr/local ..
$ make
$ make install
```

You can change the installation directory by the CMake variable _CMAKE_INSTALL_PREFIX_ or you can let it uninstalled (skip the _make install_ command). If you want to use the uninstalled version of CommonAPI set the CMake variable _USE_INSTALLED_COMMONAPI_ to _OFF_.

For further build instructions (build for windows, build documentation, tests etc.) please refer to the CommonAPI D-Bus tutorial.
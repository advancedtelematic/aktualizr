This file contains information for executing the CommonAPI-D-Bus unit tests.

Required environment variables:
-------------------------------

LD_LIBRARY_PATH should contain the folder where the dbus libraries are.
COMMONAPI_CONFIG should point to the file commonapi-dbus.ini in the src/test folder.

For DBusAddressTranslatorTest and DBusManagedTest:
The environment variable COMMONAPI_DBUS_CONFIG must be set to the absolute path to
the test-commonapi-dbus.ini file contained in the folder src/test.
The environment variable TEST_COMMONAPI_DBUS_ADDRESS_TRANSLATOR_FAKE_LEGACY_SERVICE_FOLDER
must be set to the absolute path of the folder containing the FakeLegacyService python files.

An example of the environment variable settings:

export COMMONAPI_DBUS_CONFIG=$HOME/git/ascgit017.CommonAPI-D-Bus/src/test/test-commonapi-dbus.ini
export TEST_COMMONAPI_DBUS_ADDRESS_TRANSLATOR_FAKE_LEGACY_SERVICE_FOLDER=$HOME/git/ascgit017.CommonAPI-D-Bus/src/test/fakeLegacyService
export LD_LIBRARY_PATH=$HOME/git/ascgit017.CommonAPI-D-Bus/patched-dbus/lib:$HOME/git/ascgit017.CommonAPI-D-Bus/build/src/test
export COMMONAPI_CONFIG=$HOME/git/ascgit017.CommonAPI-D-Bus/src/test/commonapi-dbus.ini

Building and executing- the tests:
----------------------------------

If you have successfully built the CommonAPI-D-Bus library with its build system, you can now built and run the tests.
Enter the build directory if the CommonAPI-D-Bus library and run

make check

This builds and runs the tests.

To build and run individual tests, enter the src/test directory under the build directory.
The individual test files are here, along with a Makefile that is able to build them.



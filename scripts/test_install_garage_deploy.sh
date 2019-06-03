#! /bin/bash

set -exuo pipefail

TEST_INSTALL_DESTDIR=${TEST_INSTALL_DESTDIR:-/persistent}

dpkg -i "$TEST_INSTALL_DESTDIR"/garage_deploy*.deb
garage-deploy --version
garage-sign --help

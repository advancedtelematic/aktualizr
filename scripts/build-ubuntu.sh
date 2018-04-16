#! /bin/bash
set -ex

GITREPO_ROOT="${1:-$(readlink -f "$(dirname "$0")/..")}"
PKG_DESTDIR="${PKG_DESTDIR:-/persistent}"
mkdir -p "${PKG_DESTDIR}"

mkdir -p build-ubuntu
cd build-ubuntu
cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_OSTREE=OFF -DBUILD_DEB=ON "${GITREPO_ROOT}"

make -j8
make package
cp aktualizr*.deb "${PKG_DESTDIR}/aktualizr.deb"
cp -rf "${GITREPO_ROOT}/tests/test_data/prov_selfupdate" "${PKG_DESTDIR}"
cp -rf "${GITREPO_ROOT}/tests/config/selfupdate.toml" "${PKG_DESTDIR}"
cp -rf "${GITREPO_ROOT}/scripts/selfupdate_server.py" "${PKG_DESTDIR}"
cp -f "${GITREPO_ROOT}/scripts/test_aktualizr_deb_and_update.sh" "${PKG_DESTDIR}/test_aktualizr_deb_and_update.sh"
cp -rf "${GITREPO_ROOT}/tests/test_data/fake_root" "${PKG_DESTDIR}"


git -C "${GITREPO_ROOT}" fetch --unshallow || true
git -C "${GITREPO_ROOT}" describe > "${PKG_DESTDIR}/aktualizr-version"

#!/bin/bash
set -euo pipefail

if [ "$#" -lt 1 ]; then
  echo "Usage: ./makephysical.sh /path/to/physical_sysroot [port]"
  exit 1
fi

TARGETDIR=$(realpath $1)
OSNAME="dummy-os"
BRANCHNAME="generated"
HARDWARE="dummy-hw"

echo "Creating OSTreee physical sysroot in ${TARGETDIR}"
rm -rf ${TARGETDIR}

mkdir -p ${TARGETDIR}
ostree admin --sysroot=${TARGETDIR} init-fs ${TARGETDIR}
ostree admin --sysroot=${TARGETDIR} os-init ${OSNAME}

mkdir -p ${TARGETDIR}/boot/loader.0
ln -s loader.0 ${TARGETDIR}/boot/loader

touch ${TARGETDIR}/boot/loader/uEnv.txt

SCRIPT_DIR="$(dirname "$0")"
OSTREE_DIR=$(mktemp -d /tmp/ostreephys-XXXXX)
${SCRIPT_DIR}/makedeployed.sh ${OSTREE_DIR}/repo ${BRANCHNAME} ${HARDWARE}

if [ "$#" -eq 2 ]; then
  echo "port: $2"
  PORT=$2
else
  PORT=56042
fi

CURDIR=$(pwd)
cd ${OSTREE_DIR}/repo

python3 -m http.server ${PORT} &
trap 'kill %1' EXIT
# Wait for http server to start serving. This can take a while sometimes.
until curl 127.0.0.1:${PORT} &> /dev/null
do
  sleep 1
done

ostree --repo=${TARGETDIR}/ostree/repo remote add --no-gpg-verify generate-remote http://127.0.0.1:${PORT} ${BRANCHNAME}
ostree --repo=${TARGETDIR}/ostree/repo pull generate-remote  ${BRANCHNAME}

cd ${CURDIR}
rm -rf ${OSTREE_DIR}
export OSTREE_BOOT_PARTITION="/boot"

ostree admin --sysroot=${TARGETDIR} deploy --os=${OSNAME} ${BRANCHNAME}

exit 0

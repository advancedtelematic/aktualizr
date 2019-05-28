#! /bin/bash
set -euo pipefail

SCRIPT_DIR="$(dirname "$0")"
DATA_DIR="${SCRIPT_DIR}/../tests/test_data"

TEST_PKCS11_MODULE_PATH=${TEST_PKCS11_MODULE_PATH:-/usr/lib/softhsm/libsofthsm2.so}
TOKEN_DIR=${TOKEN_DIR:-/var/lib/softhsm/tokens}
SOFTHSM2_CONF=${SOFTHSM2_CONF:-/etc/softhsm/softhsm2.conf}
sed -i "s:^directories\\.tokendir = .*$:directories.tokendir = ${TOKEN_DIR}:" "${SOFTHSM2_CONF}"

TMPDIR="$(mktemp -d)"
function finish {
    rm -rf "${TMPDIR}"
}
trap finish EXIT

mkdir -p "${TOKEN_DIR}"
softhsm2-util --init-token --slot 0 --label "Virtual token" --pin 1234 --so-pin 1234
SLOT=$(softhsm2-util --show-slots | grep -m 1 -oP 'Slot \K[0-9]+')
echo "Initialized token in slot: $SLOT"
softhsm2-util --import "${DATA_DIR}/device_cred_prov/pkey.pem" --label "pkey" --id 02 --slot "$SLOT" --pin 1234
openssl x509 -outform der -in "${DATA_DIR}/device_cred_prov/client.pem" -out "${TMPDIR}/client.der"
pkcs11-tool --module="${TEST_PKCS11_MODULE_PATH}" --id 1 --write-object "${TMPDIR}/client.der" --type cert --login --pin 1234

openssl pkcs8 -topk8 -inform PEM -outform PEM -nocrypt -in "${DATA_DIR}/priv.key" -out "${TMPDIR}/priv.p8"
softhsm2-util --import "${TMPDIR}/priv.p8" --label "uptane" --id 03 --slot "$SLOT" --pin 1234

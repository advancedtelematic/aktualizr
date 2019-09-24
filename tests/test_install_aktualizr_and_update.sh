#! /bin/bash

set -exuo pipefail

SCRIPT_DIR="$(dirname "$0")"
TEST_INSTALL_DESTDIR=$(mktemp -d)/install
./scripts/testupdate_server.py 0 "$TEST_INSTALL_DESTDIR"&
PORT=$("$SCRIPT_DIR/find_listening_port.sh" $!)
trap 'kill %1' EXIT

until curl 127.0.0.1:"$PORT" &> /dev/null; do
  sleep 0.2
done

$1/src/aktualizr_primary/aktualizr --version

$1/src/uptane_generator/uptane-generator generate --path "$TEST_INSTALL_DESTDIR/fake_root"
$1/src/uptane_generator/uptane-generator image --path "$TEST_INSTALL_DESTDIR/fake_root" --targetname testupdate_2.0 --filename "./tests/test_data/testupdate_2.0" --hwid testupdate-device
$1/src/uptane_generator/uptane-generator addtarget --path "$TEST_INSTALL_DESTDIR/fake_root" --targetname testupdate_2.0 --hwid testupdate-device --serial 723f79763eda1c753ce565c16862c79acdde32eb922d6662f088083c51ffde66
$1/src/uptane_generator/uptane-generator signtargets --path "$TEST_INSTALL_DESTDIR/fake_root"

TEMP_DIR=$(mktemp -d)
mkdir -m 700 -p "$TEMP_DIR/import"
cp ./tests/test_data/prov_testupdate/* "$TEMP_DIR/import"
echo -e "[storage]\\npath = \"$TEMP_DIR\"\\n[import]\\nbase_path = \"$TEMP_DIR/import\"" > "$TEMP_DIR/conf.toml"
echo -e "[tls]\\nserver = \"http://localhost:$PORT\"" >> "$TEMP_DIR/conf.toml"
$1/src/aktualizr_primary/aktualizr -c ./tests/config/testupdate.toml -c "$TEMP_DIR/conf.toml" once

# check the updated file appeared in the installation directory and sha256sum matches expectation
filename=$(echo $($1/src/aktualizr_info/aktualizr-info -c ./tests/config/testupdate.toml -c "$TEMP_DIR/conf.toml" --director-target | jq '(.signed.targets["testupdate_2.0"].hashes.sha256)') | tr [a-f] [A-F] | tr -d '"')
ls $TEMP_DIR/images/$filename
if [ $? -ne 0 ];then
    echo "ERROR: $filename does not exist or sha256sum does not match."
    exit 1
fi


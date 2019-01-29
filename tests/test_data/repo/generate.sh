#!/bin/bash

set -exuo pipefail

DEST_DIR=${1:-"."}
REPO_DIR=$DEST_DIR/repo
CMAKE_BUILD_DIR=${2:-"../../../build"}

AKTUALIZR_REPO=${AKTUALIZR_REPO:-"$CMAKE_BUILD_DIR/src/aktualizr_repo/aktualizr-repo"}

akrepo() {
    "$AKTUALIZR_REPO" --path "$DEST_DIR" "$@"
}

move_meta() {
    NAME="$1"
    mv "$REPO_DIR/director/targets.json" "$REPO_DIR/director/targets_$NAME.json"
    mv "$REPO_DIR/image/snapshot.json" "$REPO_DIR/image/snapshot_$NAME.json"
    mv "$REPO_DIR/image/targets.json" "$REPO_DIR/image/targets_$NAME.json"
    mv "$REPO_DIR/image/timestamp.json" "$REPO_DIR/image/timestamp_$NAME.json"
}

orig_meta() {
    echo "$dirtargets0" > "$REPO_DIR/director/targets.json"
    echo "$imgsnapshot0" > "$REPO_DIR/image/snapshot.json"
    echo "$imgtargets0" > "$REPO_DIR/image/targets.json"
    echo "$imgtimestamp0" > "$REPO_DIR/image/timestamp.json"
}

akrepo --command generate --expires 2021-07-04T16:33:27Z --correlationid id0

dirtargets0=$(cat "$REPO_DIR/director/targets.json")
imgsnapshot0=$(cat "$REPO_DIR/image/snapshot.json")
imgtargets0=$(cat "$REPO_DIR/image/targets.json")
imgtimestamp0=$(cat "$REPO_DIR/image/timestamp.json")

# has update
akrepo --command image --filename "$REPO_DIR/image/targets/dummy_firmware.txt"
akrepo --command image --filename "$REPO_DIR/image/targets/primary_firmware.txt"
akrepo --command image --filename "$REPO_DIR/image/targets/secondary_firmware.txt"
akrepo --command addtarget --hwid primary_hw --serial CA:FE:A6:D2:84:9D --targetname primary_firmware.txt
akrepo --command addtarget --hwid secondary_hw --serial secondary_ecu_serial --targetname secondary_firmware.txt
akrepo --command signtargets
move_meta hasupdates
orig_meta

# no update
akrepo --command image --filename "$REPO_DIR/image/targets/dummy_firmware.txt"
akrepo --command signtargets
move_meta noupdates
orig_meta

# multisec
akrepo --command image --filename "$REPO_DIR/image/targets/dummy_firmware.txt"
akrepo --command image --filename "$REPO_DIR/image/targets/secondary_firmware.txt"
akrepo --command image --filename "$REPO_DIR/image/targets/secondary_firmware2.txt"
akrepo --command addtarget --hwid sec_hwid1 --serial sec_serial1 --targetname secondary_firmware.txt
akrepo --command addtarget --hwid sec_hwid2 --serial sec_serial2 --targetname secondary_firmware2.txt
akrepo --command signtargets
move_meta multisec

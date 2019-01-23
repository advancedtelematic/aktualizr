#! /bin/bash
set -eEuo pipefail

if [ "$#" -ne 2 ]; then
  echo "Usage: $0 <aktualizr-repo> <output directory>"
  exit 1
fi

AKTUALIZR_REPO="$1"
DEST_DIR="$2"

akrepo() {
    "$AKTUALIZR_REPO" --path "$DEST_DIR" "$@"
}

if [ -d "$DEST_DIR" ]; then
    # already here, bailing
    exit 0
fi

mkdir -p "$DEST_DIR"
trap 'rm -rf "$DEST_DIR"' ERR

IMAGES=$(mktemp -d)
trap 'rm -rf "$IMAGES"' exit
PRIMARY_FIRMWARE="$IMAGES/primary.txt"
echo "primary" > "$PRIMARY_FIRMWARE"
SECONDARY_FIRMWARE="$IMAGES/secondary.txt"
echo "secondary" > "$SECONDARY_FIRMWARE"

akrepo --command generate --expires 2021-07-04T16:33:27Z
akrepo --command image --filename "$PRIMARY_FIRMWARE" --targetname primary.txt
akrepo --command image --filename "$SECONDARY_FIRMWARE" --targetname secondary.txt
akrepo --command addtarget --hwid primary_hw --serial CA:FE:A6:D2:84:9D --filename primary.txt
akrepo --command addtarget --hwid secondary_hw --serial secondary_ecu_serial --filename secondary.txt
akrepo --command signtargets

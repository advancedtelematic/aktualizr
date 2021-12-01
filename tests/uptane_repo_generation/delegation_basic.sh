#! /bin/bash
set -eEuo pipefail

if [ "$#" -lt 2 ]; then
  echo "Usage: $0 <uptane-generator> <output directory> <revoke>"
  exit 1
fi

UPTANE_GENERATOR="$1"
DEST_DIR="$2"
REVOKE=""
if [ "$#" -eq 3 ]; then
  REVOKE="$3"
fi


uptane_gen() {
    "$UPTANE_GENERATOR" --path "$DEST_DIR" "$@"
}

mkdir -p "$DEST_DIR"
trap 'rm -rf "$DEST_DIR"' ERR

IMAGES=$(mktemp -d)
trap 'rm -rf "$IMAGES"' exit
PRIMARY_FIRMWARE="$IMAGES/primary.txt"
echo "primary" > "$PRIMARY_FIRMWARE"
SECONDARY_FIRMWARE="$IMAGES/secondary.txt"
echo "secondary" > "$SECONDARY_FIRMWARE"
if [[ "$REVOKE" = "revoke" ]]; then
    uptane_gen --command revokedelegation --dname new-role
else
    uptane_gen --command generate --expires 2025-07-04T16:33:27Z
    uptane_gen --command adddelegation --dname new-role --dpattern "abc/*" --keytype ed25519
    uptane_gen --command image --filename "$PRIMARY_FIRMWARE" --targetname primary.txt --hwid primary_hw
    uptane_gen --command image --filename "$SECONDARY_FIRMWARE" --targetname "abc/secondary.txt" --dname new-role --hwid secondary_hw
    uptane_gen --command addtarget --hwid primary_hw --serial CA:FE:A6:D2:84:9D --targetname primary.txt
    uptane_gen --command addtarget --hwid secondary_hw --serial secondary_ecu_serial --targetname "abc/secondary.txt"
    uptane_gen --command signtargets
fi

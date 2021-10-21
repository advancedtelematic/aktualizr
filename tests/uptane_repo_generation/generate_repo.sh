#! /bin/bash
set -eEuo pipefail

if [ "$#" -eq 0 ]; then
  echo "Usage: $0 <uptane-generator> <output directory> [options]"
  echo "Options:"
  echo "  -s|--add_default_secondary"
  echo "  -c|--add_campaigns"
  exit 1
fi

ADD_DEFAULT_SECONDARY=false
ADD_CAMPAIGNS=false

for arg in "$@"
do
    case $arg in
        -s|--add_default_secondary)
        ADD_DEFAULT_SECONDARY=true
        ;;
        -c|--add_campaigns)
        ADD_CAMPAIGNS=true
        ;;
    esac
done

UPTANE_GENERATOR="$1"
DEST_DIR="$2"

uptane_gen() {
    "$UPTANE_GENERATOR" --path "$DEST_DIR" "$@"
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

uptane_gen --command generate --expires 2025-07-04T16:33:27Z
uptane_gen --command image --filename "$PRIMARY_FIRMWARE" --targetname primary.txt --hwid primary_hw
uptane_gen --command addtarget --hwid primary_hw --serial CA:FE:A6:D2:84:9D --targetname primary.txt

if [ "$ADD_DEFAULT_SECONDARY" = true ]; then
    SECONDARY_FIRMWARE="$IMAGES/secondary.txt"
    echo "secondary" > "$SECONDARY_FIRMWARE"
    uptane_gen --command image --filename "$SECONDARY_FIRMWARE" --targetname secondary.txt --hwid secondary_hw
    uptane_gen --command addtarget --hwid secondary_hw --serial secondary_ecu_serial --targetname secondary.txt
fi

uptane_gen --command signtargets

if [ "$ADD_CAMPAIGNS" = true ]; then
    uptane_gen --command addcampaigns
fi

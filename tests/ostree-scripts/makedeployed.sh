#!/bin/bash
set -euo pipefail

if [ "$#" -ne 3 ]; then
  echo "Usage: ./makedeployed.sh /path/to/ostree/repo branchname hardware_id"
  exit 1
fi

REPO=$1
BRANCHNAME=$2
HARDWARE=$3

TARGETDIR=$(mktemp -d /tmp/genostree-XXXXXX)

#usr
mkdir -p "$TARGETDIR/usr/share/sota"
echo "$BRANCHNAME" > "$TARGETDIR/usr/share/sota/branchname"

#var
mkdir -p "$TARGETDIR/var/sota"

#etc
mkdir -p "$TARGETDIR/usr/etc"
echo "$HARDWARE" > "$TARGETDIR/usr/etc/hostname"
cat << EOF > "$TARGETDIR/usr/etc/os-release"
ID="dummy-os"
NAME="Generated OSTree-enabled OS"
VERSION="3.14159"
EOF

#boot
mkdir -p "$TARGETDIR/boot/loader.0"
mkdir -p "$TARGETDIR/boot/loader.1"

ln -sf boot/loader.0 "$TARGETDIR/boot/loader"
echo "I'm a kernel" > "$TARGETDIR/boot/vmlinuz"
echo "I'm an initrd" > "$TARGETDIR/boot/initramfs"

checksum=$(sha256sum "$TARGETDIR/boot/vmlinuz" | cut -f 1 -d " ")
mv "$TARGETDIR/boot/vmlinuz" "$TARGETDIR/boot/vmlinuz-$checksum"
mv "$TARGETDIR/boot/initramfs" "$TARGETDIR/boot/initramfs-$checksum"

if [ ! -d "$REPO" ]; then
  ostree --repo="$REPO" init --mode=archive-z2
fi

ostree --repo="$REPO" commit \
       --tree=dir="$TARGETDIR" \
       --branch="$BRANCHNAME" \
       --subject="Minimal OSTree deployment"

rm -rf "$TARGETDIR"

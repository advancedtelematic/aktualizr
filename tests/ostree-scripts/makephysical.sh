#!/bin/bash
set -euo pipefail

if [ "$#" -lt 1 ]; then
  echo "Usage: ./makephysical.sh /path/to/physical_sysroot [port]"
  exit 1
fi

OSTREE_SYSROOT="$(realpath "$1")"  # replaces `--sysroot=...`
export OSTREE_SYSROOT
OSNAME="dummy-os"
BRANCHNAME="generated"
HARDWARE="dummy-hw"

echo "Creating OSTreee physical sysroot in $OSTREE_SYSROOT"
rm -rf "$OSTREE_SYSROOT"

mkdir -p "$OSTREE_SYSROOT"
ostree admin init-fs "$OSTREE_SYSROOT"
ostree admin os-init $OSNAME
ostree config --repo="${OSTREE_SYSROOT}/ostree/repo" set core.mode bare-user-only

mkdir -p "$OSTREE_SYSROOT/boot/loader.0"
ln -s loader.0 "$OSTREE_SYSROOT/boot/loader"

touch "$OSTREE_SYSROOT/boot/loader/uEnv.txt"

SCRIPT_DIR="$(dirname "$0")"
OSTREE_DIR=$(mktemp -d /tmp/ostreephys-XXXXXX)
"$SCRIPT_DIR/makedeployed.sh" "$OSTREE_DIR/repo" $BRANCHNAME $HARDWARE

(
    cd "$OSTREE_DIR/repo"

    python3 -m http.server 0 &
    PORT=$("$SCRIPT_DIR/../find_listening_port.sh" $!)
    trap 'kill %1' EXIT

    # Wait for http server to start serving. This can take a while sometimes.
    until curl localhost:"$PORT" &> /dev/null; do
      sleep 0.2
    done

    ostree --repo="$OSTREE_SYSROOT/ostree/repo" remote add --no-gpg-verify generate-remote "http://localhost:$PORT" $BRANCHNAME
    ostree --repo="$OSTREE_SYSROOT/ostree/repo" pull generate-remote  $BRANCHNAME
)

rm -rf "$OSTREE_DIR"
export OSTREE_BOOT_PARTITION="/boot"

ostree admin deploy --os=$OSNAME $BRANCHNAME

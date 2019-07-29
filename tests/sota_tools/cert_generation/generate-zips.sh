#!/bin/bash
set -eEuo pipefail

if [ "$#" -lt 1 ]; then
  echo "Usage: $0 <output directory>"
  exit 1
fi

SRC_DIR=$(dirname "$0")
DEST_DIR="$1"

mkdir -p "$DEST_DIR"
trap 'rm -rf "$DEST_DIR"' ERR

cp "$SRC_DIR/client_good.p12" "$DEST_DIR/client_auth.p12"
zip -j "$DEST_DIR/good.zip" "$DEST_DIR/client_auth.p12" "$SRC_DIR/api_gateway.url"
rm "$DEST_DIR/client_auth.p12"

cp "$SRC_DIR/client_bad.p12" "$DEST_DIR/client_auth.p12"
zip -j "$DEST_DIR/bad.zip" "$DEST_DIR/client_auth.p12" "$SRC_DIR/api_gateway.url"
rm "$DEST_DIR/client_auth.p12"

#!/usr/bin/env bash
set -e

TEMP_DIR=$(mktemp -d)
cp -rf test_data/aktualizr-info.db $TEMP_DIR
echo "[storage]" > $TEMP_DIR/conf.toml

echo "path = \"$TEMP_DIR\"" >> $TEMP_DIR/conf.toml
echo "uptane_metadata_path = \"metadata\"" >> $TEMP_DIR/conf.toml
echo "sqldb_path = \"$TEMP_DIR/aktualizr-info.db\"" >> $TEMP_DIR/conf.toml

$@ -c $TEMP_DIR/conf.toml

rm -rf $TEMP_DIR
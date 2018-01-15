#!/usr/bin/env bash
set -e

TEMP_DIR=$(mktemp -d)
cp -rf test_data/prov/* $TEMP_DIR
echo "[storage]" > $TEMP_DIR/conf.toml

echo "path = \"$TEMP_DIR\"" >> $TEMP_DIR/conf.toml
echo "uptane_metadata_path = \"metadata\"" >> $TEMP_DIR/conf.toml
echo "schemas_path = \"../config/storage\"" >> $TEMP_DIR/conf.toml
echo "sqldb_path = \"$TEMP_DIR/database.db\"" >> $TEMP_DIR/conf.toml

$@ -c $TEMP_DIR/conf.toml

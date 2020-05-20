#!/bin/bash -e

TEMP_DIR=$(mktemp -d)/aktualizr
MEMORY_LIMIT=10485760 #10 Mib

trap 'kill %1; rm -rf $TEMP_DIR' EXIT

AKTUALIZR=$1
UPTANE_GENERATOR=$2
BUILD_DIR=$3
PATH="$(dirname "$UPTANE_GENERATOR"):$PATH"
./src/uptane_generator/run/create_repo.sh "$TEMP_DIR" localhost

METRICS_FILE="$BUILD_DIR/metrics.txt"
TARGET_SIZE="200M"

mkdir -p "$TEMP_DIR/deb/DEBIAN"
mkdir -p "$TEMP_DIR/deb/usr/share"
fallocate -l $TARGET_SIZE "$TEMP_DIR/deb/usr/share/target1"

cat > "$TEMP_DIR/deb/DEBIAN/control" <<EOF
Package: testdeb
Version: 1.1-1
Section: base
Priority: optional
Architecture: all
Maintainer: Serhiy Stetskovych <test@mal.com>
Description: Some fake package
 This is fake package.
EOF

dpkg-deb -Znone -b "$TEMP_DIR/deb" "$TEMP_DIR/good.deb"
PATH="tests/test_data/fake_dpkg:$PATH"
"$UPTANE_GENERATOR" image --path "$TEMP_DIR/uptane" --filename "$TEMP_DIR/good.deb" --targetname good.deb --hwid desktop
"$UPTANE_GENERATOR" addtarget --path "$TEMP_DIR/uptane" --targetname good.deb --hwid desktop --serial serial1
"$UPTANE_GENERATOR" signtargets --path "$TEMP_DIR/uptane"

sed -i 's/\[provision\]/\[provision\]\nprimary_ecu_serial = serial1/g'  "$TEMP_DIR/sota.toml"
sed -i 's/type = "none"/type = "debian"/g' "$TEMP_DIR/sota.toml"

./src/uptane_generator/run/serve_repo.py 9000 "$TEMP_DIR" &

valgrind --tool=massif --massif-out-file="$BUILD_DIR/massif.out" "$AKTUALIZR" --loglevel 1 --config "$TEMP_DIR/sota.toml" --run-mode once

HEAP_MEMORY=$(grep -oP 'mem_heap_B=\K.*' "$BUILD_DIR/massif.out" | sort -nr | head -n 1)
echo -n "Heap memory usage is "
echo "$HEAP_MEMORY" | numfmt --to=iec-i

line="massif_install_mem_peak $HEAP_MEMORY"
if grep -q '^massif_install_mem' "$METRICS_FILE" 2> /dev/null; then
    sed -i 's/^massif_install_mem.*$/'"$line" "$METRICS_FILE"
else
    echo "$line" >> "$METRICS_FILE"

fi

if [ "$HEAP_MEMORY" -gt "$MEMORY_LIMIT" ]; then
    echo "Test fail because of memory consumption is greater then $MEMORY_LIMIT bytes"
    exit 1
fi

#!/bin/bash -e

TEMP_DIR=$(mktemp -d)/aktualizr
MEMMORY_LIMIT=104857600 #100 Mib

trap 'kill %1; rm -rf $TEMP_DIR' EXIT

PATH=$PATH:"$1/src/aktualizr_repo/"
./src/aktualizr_repo/run/create_repo.sh $TEMP_DIR localhost

TARGET_SIZE="200M"
fallocate -l $TARGET_SIZE target1

$1/src/aktualizr_repo/aktualizr-repo image --filename target1 --path $TEMP_DIR/uptane
$1/src/aktualizr_repo/aktualizr-repo addtarget --filename target1 --path $TEMP_DIR/uptane --hwid desktop --serial serial1
$1/src/aktualizr_repo/aktualizr-repo signtargets  --path $TEMP_DIR/uptane


sed -i 's/\[provision\]/\[provision\]\nprimary_ecu_serial = serial1/g'  "$TEMP_DIR/sota.toml"

./src/aktualizr_repo/run/serve_repo.py 9000 "$TEMP_DIR" &

valgrind --tool=massif --massif-out-file=$1/massif.out $1/src/aktualizr_primary/aktualizr --loglevel 1 --config "$TEMP_DIR/sota.toml" --run-mode once

HEAP_MEMMORY=$(grep -oP 'mem_heap_B=\K.*' $1/massif.out | sort -nr | head -n 1)
echo -n "Heap memory usage is "
echo $HEAP_MEMMORY | numfmt --to=iec-i
if [ "$HEAP_MEMMORY" -gt "$MEMMORY_LIMIT" ]; then
    echo "Test fail because of memory consumption is greater then $MEMMORY_LIMIT bytes"
    exit 1
fi

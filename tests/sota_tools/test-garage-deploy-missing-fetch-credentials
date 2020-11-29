#!/bin/bash
set -eu
TARGET="Unable to read a-non-existent-file as archive or json file"
# File not found and invalid/corrupt files are treated the same
$1 --commit 16ef2f2629dc9263fdf3c0f032563a2d757623bbc11cf99df25c3c3f258dccbe -f a-non-existent-file -p "$2" --name testname -h hwids 2>&1 | grep "$TARGET"

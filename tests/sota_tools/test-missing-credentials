#!/bin/bash
set -eu

TARGET="Unable to read a-non-existent-file as archive or json file"
# File not found and invalid/corrupt files are treated the same
$1 --ref master --repo sota_tools/repo --credentials a-non-existent-file | grep -q "$TARGET"

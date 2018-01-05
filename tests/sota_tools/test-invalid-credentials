#!/bin/bash
set -eu

TARGET="Unable to read invalid-credentials.zip as archive or json file"
$1 --ref master --repo sota_tools/repo --credentials invalid-credentials.zip | grep -q "$TARGET"

#!/bin/bash
set -eu

TARGET="cacert path nope.pem does not exist"
$1 --ref master --cacert nope.pem --repo sota_tools/repo --credentials sota_tools/sota_tools_basic_auth.json -n | grep -q "$TARGET"

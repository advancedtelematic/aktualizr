#!/bin/bash
set -eu

TARGET="Ref or commit refhash badref was not found in repository sota_tools/repo"
$1 --ref badref --repo sota_tools/repo --credentials "$2" | grep -q "$TARGET"

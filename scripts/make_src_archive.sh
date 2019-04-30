#!/usr/bin/env bash

set -eu

REPO=$1
OUTPUT=$2

pip3 install --user "https://files.pythonhosted.org/packages/60/44/26791beafa5c2c1f8ac0dd38a7761a0162c0f31bdad91a0091a48a923a44/git_archive_all-1.19.4-py2.py3-none-any.whl"

cd "$REPO"
git-archive-all "$OUTPUT"

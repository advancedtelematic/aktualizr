#!/usr/bin/env bash

set -eu

OUTPUT=$1
REPO=${2:-.}

python3 -m venv venv

. venv/bin/activate

pip install 'git_archive_all==1.19.4'

cd "$REPO"
git-archive-all "$OUTPUT"

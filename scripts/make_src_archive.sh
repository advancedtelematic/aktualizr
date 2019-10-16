#!/usr/bin/env bash

set -eu

OUTPUT=$1
REPO=${2:-.}

python3 -m venv venv

. venv/bin/activate

pip install 'git_archive_all==1.19.4'

ABS_OUT=$(realpath "$OUTPUT")

TMPDIR=$(mktemp -d)
trap 'rm -rf $TMPDIR' EXIT
cp -r "$REPO" "$TMPDIR/aktualizr"

cd "$TMPDIR/aktualizr"
# include version in tarball
git describe --long | tr -d '\n' > VERSION

git-archive-all "$ABS_OUT" --extra=VERSION

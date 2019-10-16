#!/usr/bin/env bash

set -eu

SCRIPTS_DIR=$(readlink -f "$(dirname "$0")")

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
"$SCRIPTS_DIR/get_version.sh" > VERSION

git-archive-all "$ABS_OUT" --extra=VERSION

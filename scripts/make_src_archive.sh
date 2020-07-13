#!/usr/bin/env bash

set -eux

SCRIPTS_DIR=$(readlink -f "$(dirname "$0")")

OUTPUT=$(realpath "$1")
REPO=$(realpath "${2:-.}")

# Just in case this wasn't done before.
git -C "$REPO" submodule update --init --recursive

python3 -m venv venv

. venv/bin/activate

pip install 'git_archive_all==1.21.0'

TMPDIR=$(mktemp -d)
trap 'rm -rf $TMPDIR' EXIT
cd "$TMPDIR"

# store version in archive
"$SCRIPTS_DIR/get_version.sh" git "$REPO" > VERSION

git-archive-all -C "$REPO" --extra VERSION "$OUTPUT"

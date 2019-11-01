#!/usr/bin/env bash

set -eux

SCRIPTS_DIR=$(readlink -f "$(dirname "$0")")

OUTPUT=$(realpath "$1")
REPO=$(realpath "${2:-.}")

python3 -m venv venv

. venv/bin/activate

# Use our fork which makes it easier to add extra files
# pip install 'git_archive_all==1.19.4'
pip install git+https://github.com/Kentzo/git-archive-all@1f7938cd6db76bfcf7eb1046ccb818df72b141ad

TMPDIR=$(mktemp -d)
trap 'rm -rf $TMPDIR' EXIT
cd "$TMPDIR"

# store version in archive
"$SCRIPTS_DIR/get_version.sh" git "$REPO" > VERSION

git-archive-all -C "$REPO" --extra VERSION "$OUTPUT"

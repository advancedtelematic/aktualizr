#! /bin/bash
set -euo pipefail

if [ "$#" -ne 2 ]; then
  echo "Usage: ./make_ostree_sysroot.sh <project source dir> <project binary dir>"
  exit 1
fi

SCRIPT="${1}/tests/ostree-scripts/makephysical.sh"
REPO="${2}/ostree_repo"
PORT=$("${1}/tests/get_open_port.py")

if [ ! -d "${REPO}" ]; then
  "${SCRIPT}" "${REPO}" ${PORT}
fi

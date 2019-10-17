#!/usr/bin/env bash

set -euo pipefail

GIT=${1:-git}
REPO=${2:-.}

"$GIT" -C "$REPO" describe --long | tr -d '\n'

#!/usr/bin/env bash

set -euo pipefail

GIT=${1:-git}

"$GIT" describe --long | tr -d '\n'

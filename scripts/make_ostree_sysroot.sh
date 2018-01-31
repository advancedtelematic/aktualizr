#! /bin/bash
set -euo pipefail

if [ ! -d $2 ]; then
  $1 $2
fi

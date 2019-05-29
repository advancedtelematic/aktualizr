#!/usr/bin/env bash

set -euo pipefail

JSONCPP_SRCDIR=$1
DSTDIR=$2

TMPDIR=$(mktemp -d)
trap 'rm -rf $TMPDIR' EXIT

python3 "$JSONCPP_SRCDIR/amalgamate.py" -t "$JSONCPP_SRCDIR" -s "$TMPDIR/jsoncpp.cc" > /dev/null

if [ -d "$DSTDIR" ]; then
    if diff -rq "$DSTDIR" "$TMPDIR" > /dev/null; then
        # output already matches amalgated source
        exit 0
    fi
    rm -rf "$DSTDIR"
fi

cp -r "$TMPDIR" "$DSTDIR"

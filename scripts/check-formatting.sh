#! /bin/bash
set -euo pipefail

CLANG_FORMAT=$1
SRC_FILE=$2

DIFF=$(mktemp)
cleanup() {
    rm "$DIFF"
}
trap cleanup EXIT

FAIL=0
diff -u "$SRC_FILE" <("$CLANG_FORMAT" -style=file "$SRC_FILE") > "$DIFF" || FAIL=1
if [ ${FAIL} -eq 1 ]; then
    echo "$CLANG_FORMAT found unformatted code! Run 'make qa' before continuing."
    cat "$DIFF"
    exit 1
fi


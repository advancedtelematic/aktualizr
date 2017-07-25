#! /bin/bash
set -euo pipefail

CLANG_FORMAT=$1
shift
FILES=$@

FAIL=0
diff -u <(cat ${FILES}) <(${CLANG_FORMAT} -style=file ${FILES}) > /dev/null || FAIL=1
if [ ${FAIL} -eq 1 ]; then
    echo "${CLANG_FORMAT} found unformatted code! Run 'make qa' before continuing."
    exit 1
fi


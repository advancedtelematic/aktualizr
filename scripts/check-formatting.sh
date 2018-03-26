#! /bin/bash
set -euo pipefail

CLANG_FORMAT=$1
shift
FILES=$@

DIFF=$(tempfile)
FAIL=0
diff -u <(cat ${FILES}) <(${CLANG_FORMAT} -style=file ${FILES}) > ${DIFF} || FAIL=1
if [ ${FAIL} -eq 1 ]; then
    echo "${CLANG_FORMAT} found unformatted code! Run 'make qa' before continuing."
    cat ${DIFF}
    exit 1
fi


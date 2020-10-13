#! /bin/bash

set -euo pipefail

CLANG_TIDY="${1}"
CMAKE_BINARY_DIR="${2}"
CMAKE_SOURCE_DIR="${3}"
FILE="${4}"

if [[ ! -e "${CMAKE_BINARY_DIR}/compile_commands.json" ]]; then
  echo "compile_commands.json not found!"
  exit 1
fi

# Filter out anything that we aren't trying to compile. Older clang-tidy
# versions (<=6) skipped them automatically.
if grep -q "${FILE}" "${CMAKE_BINARY_DIR}/compile_commands.json"; then
  ${CLANG_TIDY} -quiet -header-filter="\(${CMAKE_SOURCE_DIR}|\\.\\.\)/src/.*" --extra-arg-before=-Wno-unknown-warning-option -format-style=file \
    --checks=-clang-analyzer-cplusplus.NewDeleteLeaks,-clang-analyzer-core.NonNullParamChecker \
    -p "${CMAKE_BINARY_DIR}" "${FILE}"
else
  echo "Skipping ${FILE}"
fi

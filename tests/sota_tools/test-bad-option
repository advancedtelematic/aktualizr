#!/bin/bash
set -uo pipefail

$1 --a-bad-option
# Invert the return code
test $? -eq 1

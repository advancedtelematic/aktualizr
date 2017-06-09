#!/usr/bin/env bash
set -e
virtualenv -p python3 venv
. venv/bin/activate
pip install -r "$1/requirements.txt"
$1/generator.py -t uptane --signature-encoding base64 -o vectors --cjson json-subset
$1/server.py --path "$(pwd)/vectors" &
sleep 3
trap 'kill %1' EXIT
./aktualizr_uptane_vector_tests vectors/vector-meta.json
RES=$?
kill %1
trap - EXIT
trap
exit ${RES}
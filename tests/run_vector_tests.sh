#!/usr/bin/env bash
set -e
virtualenv -p python3 venv
. venv/bin/activate
pip install -r "$1/requirements.txt"
$1/generator.py -t uptane --signature-encoding base64 -o vectors --cjson json-subset
$1/server.py --path "$(pwd)/vectors" &
sleep 3
trap 'kill %1' EXIT

if [ "$2" == "valgrind" ]; then
    valgrind --track-origins=yes --leak-check=full --error-exitcode=1 --suppressions=$1/../aktualizr.supp ./aktualizr_uptane_vector_tests vectors/vector-meta.json
else
    ./aktualizr_uptane_vector_tests vectors/vector-meta.json
fi

RES=$?
kill %1
trap - EXIT
trap
exit ${RES}
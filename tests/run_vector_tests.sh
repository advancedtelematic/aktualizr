#!/usr/bin/env bash
set -e

if [ ! -f venv/bin/activate ]; then
  python3 -m venv venv
fi

. venv/bin/activate

pip install wheel
pip install -r "$1/requirements.txt"

PORT=`$1/../get_open_port.py`

$1/generator.py -t uptane --signature-encoding base64 -o vectors --cjson json-subset
if [ "$2" == "valgrind" ]; then
  $1/server.py -t uptane --signature-encoding base64 -P $PORT &
else
  $1/server.py -t uptane --signature-encoding base64 -P $PORT &
fi
sleep 3
trap 'kill %1' EXIT

if [ "$2" == "valgrind" ]; then
    valgrind --track-origins=yes --show-possibly-lost=no --error-exitcode=1 --suppressions=$1/../aktualizr.supp ./aktualizr_uptane_vector_tests vectors/vector-meta.json $PORT
else
    ./aktualizr_uptane_vector_tests vectors/vector-meta.json $PORT
fi

RES=$?

rm -rf vectors

kill %1
trap - EXIT
trap
exit ${RES}

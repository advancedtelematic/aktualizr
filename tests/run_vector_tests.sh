#!/usr/bin/env bash
set -e

: ${1:?'missing arg'}

if [ ! -f venv/bin/activate ]; then
  python3 -m venv venv
fi

. venv/bin/activate

pip install wheel
pip install -r "$1/requirements.txt"

PORT=`$1/../get_open_port.py`


$1/server.py -t uptane --signature-encoding base64 -P $PORT \
    --cjson json-subset \
    --hardware-id FIXME --ecu-identifier FIXMETOO &

while ! curl "localhost:$PORT"; do
    sleep 0.1
done

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

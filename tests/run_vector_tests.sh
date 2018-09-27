#!/usr/bin/env bash

set -e

if [ ! -f venv/bin/activate ]; then
  python3 -m venv venv
fi

. venv/bin/activate

TESTS_SRC_DIR=${1:-}
shift 1
if [[ $1 = "valgrind" ]]; then
    WITH_VALGRIND=1
    shift 1
else
    WITH_VALGRIND=0
fi

TTV_DIR="$TESTS_SRC_DIR/tuf-test-vectors"

# use `python -m pip` to avoid problem with long shebangs on travis
python -m pip install wheel
python -m pip install -r "$TTV_DIR/requirements.txt"

PORT=$("$TESTS_SRC_DIR/get_open_port.py")
ECU_SERIAL=test_primary_ecu_serial
HARDWARE_ID=test_primary_hardware_id

"$TTV_DIR/generator.py" --signature-encoding base64 -o vectors --cjson json-subset \
                        --ecu-identifier $ECU_SERIAL --hardware-id $HARDWARE_ID
# disable werkzeug debug pin which causes issues on Jenkins
WERKZEUG_DEBUG_PIN=off "$TTV_DIR/server.py" --signature-encoding base64 -P "$PORT" \
                                            --ecu-identifier $ECU_SERIAL --hardware-id $HARDWARE_ID &
trap 'kill %1' EXIT

# wait for server to go up
tries=0
while ! curl -I -s -f "http://localhost:$PORT"; do
    if [[ $tries -ge 10 ]]; then
        echo "tuf-test-vectors server did not go up"
        exit 1
    fi
    sleep 1
    tries=$((tries+1))
done

# TODO: Why doesn't gtest_output work?
if [[ $WITH_VALGRIND = 1 ]]; then
    valgrind --track-origins=yes \
             --show-possibly-lost=no \
             --error-exitcode=1 \
             --suppressions="$TESTS_SRC_DIR/aktualizr.supp" \
             --suppressions="$TESTS_SRC_DIR/glib.supp" \
             ./aktualizr_uptane_vector_tests "$PORT" "$@"
else
    ./aktualizr_uptane_vector_tests "$PORT" "$@"
fi

RES=$?

rm -rf vectors

kill %1
trap - EXIT
trap
exit ${RES}

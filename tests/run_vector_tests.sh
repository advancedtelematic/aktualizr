#!/usr/bin/env bash
set -e

if [ ! -f venv/bin/activate ]; then
  python3 -m venv venv
fi

. venv/bin/activate

TTV_DIR="$1/tuf-test-vectors"

# use `python -m pip` to avoid problem with long shebangs on travis
python -m pip install wheel
python -m pip install -r "$TTV_DIR/requirements.txt"

PORT=$("$1/get_open_port.py")
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
if [ "$2" == "valgrind" ]; then
    valgrind --track-origins=yes \
             --show-possibly-lost=no \
             --error-exitcode=1 \
             --suppressions="$1/aktualizr.supp" \
             --suppressions="$1/glib.supp" \
             ./aktualizr_uptane_vector_tests "$PORT" --gtest_output="xml:results/"
else
    ./aktualizr_uptane_vector_tests "$PORT" --gtest_output="xml:results/"
fi

RES=$?

rm -rf vectors

kill %1
trap - EXIT
trap
exit ${RES}

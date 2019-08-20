#!/usr/bin/env bash

set -e

echo "$@"
VALGRIND=""
UPTANE_VECTOR_TEST=./aktualizr_uptane_vector_tests
while getopts "t:s:v:-" opt; do
    case "$opt" in
        t)
            UPTANE_VECTOR_TEST=$OPTARG
            ;;
        s)
            TESTS_SRC_DIR=$OPTARG
            ;;
        v)
            VALGRIND=$OPTARG
            ;;
        -)
            # left-overs arguments
            break
            ;;
        *)
            ;;
    esac
done

if [ ! -f venv/bin/activate ]; then
  python3 -m venv venv
fi

. venv/bin/activate

TTV_DIR="$TESTS_SRC_DIR/tuf-test-vectors"

# use `python -m pip` to avoid problem with long shebangs on travis
python -m pip install wheel
python -m pip install -r "$TTV_DIR/requirements.txt"

ECU_SERIAL=test_primary_ecu_serial
HARDWARE_ID=test_primary_hardware_id

"$TTV_DIR/generator.py" --signature-encoding base64 -o vectors --cjson json-subset \
                        --ecu-identifier $ECU_SERIAL --hardware-id $HARDWARE_ID
"$TTV_DIR/server.py" --signature-encoding base64 -P 0 \
                     --ecu-identifier $ECU_SERIAL --hardware-id $HARDWARE_ID &
PORT=$("$TESTS_SRC_DIR/find_listening_port.sh" $!)
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

if [[ -n $VALGRIND ]]; then
    "$VALGRIND" "$UPTANE_VECTOR_TEST" "$PORT" "$@"
else
    "$UPTANE_VECTOR_TEST" "$PORT" "$@"
fi

RES=$?

rm -rf vectors

kill %1
trap - EXIT
trap
exit ${RES}

#!/usr/bin/env bash
set -xeuo pipefail

TEMP_DIR=/tmp/temp_aktualizr_expire_repo/$(mktemp -d)/$1
"$2/src/uptane_generator/uptane-generator" generate $TEMP_DIR --expires=$1
"$2/src/uptane_generator/uptane-generator" image $TEMP_DIR ./tests/test_data/credentials.zip --hwid test_hwid

cp ./tests/test_data/credentials.zip $TEMP_DIR

PORT=$(python -c 'import socket; s=socket.socket(); s.bind(("", 0)); print(s.getsockname()[1]); s.close()')
TREEHUB="{\
  \"oauth2\": {
    \"server\": \"http://localhost:$PORT\",\
    \"client_id\": \"54515188-64d1-48bc-bd63-fb707f934fd8\",\
    \"client_secret\": \"SVgpRfS11q\"\
  },\
  \"ostree\": {\
    \"server\": \"http://localhost:$PORT/\"\
  }\
}"

echo $TREEHUB > $TEMP_DIR/treehub.json
echo "http://localhost:$PORT" > $TEMP_DIR/tufrepo.url

pushd $TEMP_DIR
zip -r $TEMP_DIR/credentials.zip  treehub.json tufrepo.url
popd

function finish {
  kill %1
  rm -rf $TEMP_DIR
}

./tests/fake_http_server/fake_api_server.py $PORT $TEMP_DIR &
trap finish EXIT
sleep 2

"$2/src/sota_tools/garage-check" -j $TEMP_DIR/credentials.zip -r 714581b2ffbbf7a750cb0a210fa7d74fd9128bd627cd4913e365d5bf2f66eba9

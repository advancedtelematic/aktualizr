#! /bin/bash
set -eEuo pipefail

if [ "$#" -lt 3 ]; then
  echo "Usage: $0 <aktualizr-repo> <output directory> <port>"
  exit 1
fi

AKTUALIZR_REPO="$1"
DEST_DIR="$2"
PORT="$3"

akrepo() {
    "$AKTUALIZR_REPO" --path "$DEST_DIR" "$@"
}

mkdir -p "$DEST_DIR"
trap 'rm -rf "$DEST_DIR"' ERR

IMAGES=$(mktemp -d)
trap 'rm -rf "$IMAGES"' exit
DOCKER_APP="$IMAGES/foo.dockerapp"
echo "fake contents of a docker app" > "$DOCKER_APP"

akrepo --command generate --expires 2021-07-04T16:33:27Z
akrepo --command image --filename "$DOCKER_APP" --targetname foo.dockerapp
akrepo --command addtarget --hwid primary_hw --serial CA:FE:A6:D2:84:9D --targetname foo.dockerapp
akrepo --command signtargets

cd $DEST_DIR
echo "Target.json is: "
cat repo/image/targets.json
echo "Running repo server port on $PORT"
exec python3 -m http.server $PORT

#! /bin/bash
set -eEuo pipefail

if [ "$#" -lt 3 ]; then
  echo "Usage: $0 <uptane-generator> <output directory> <port>"
  exit 1
fi

UPTANE_GENERATOR="$1"
DEST_DIR="$2"
PORT="$3"

uptane_gen() {
    "$UPTANE_GENERATOR" --path "$DEST_DIR" "$@"
}

mkdir -p "$DEST_DIR"
trap 'rm -rf "$DEST_DIR"' ERR

IMAGES=$(mktemp -d)
trap 'rm -rf "$IMAGES"' exit
DOCKER_APP="$IMAGES/foo.dockerapp"
echo "fake contents of a docker app" > "$DOCKER_APP"

uptane_gen --command generate --expires 2021-07-04T16:33:27Z
uptane_gen --command image --filename "$DOCKER_APP" --targetname foo.dockerapp --hwid primary_hw
uptane_gen --command addtarget --hwid primary_hw --serial CA:FE:A6:D2:84:9D --targetname foo.dockerapp
uptane_gen --command signtargets

cd $DEST_DIR
echo "Target.json is: "
cat repo/repo/targets.json
echo "Running repo server port on $PORT"
exec python3 -m http.server $PORT

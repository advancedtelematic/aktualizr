#!/usr/bin/env bash
set -e

build_dir=$(pwd)
aklite=$1
akrepo_bin=$2
tests_dir=$3
#valgrind=$4
valgrind=""

dest_dir=$(mktemp -d)

cleanup() {
    echo "cleaning up temp dir"
    rm -rf "$dest_dir"
    if [ -n "$pid" ] ; then
        echo "killing webserver"
        kill $pid
    fi
}
trap cleanup EXIT

akrepo() {
    $akrepo_bin --repotype image --path "$dest_dir" "$@"
}

add_target() {
    custom_json="${dest_dir}/custom.json"
    name=$1
    if [ -n "$2" ] ; then
        sha=$2
    else
        sha=$(echo $name | sha256sum | cut -f1 -d\  )
    fi
    cat >$custom_json <<EOF
{
  "hardwareIds": ["hwid-for-test"],
  "targetFormat": "OSTREE"
}
EOF
    akrepo --command image \
        --targetname $name --targetsha256 $sha --targetlength 0 --targetcustom $custom_json
}

akrepo --command generate --expires 2021-07-04T16:33:27Z
add_target foo1
add_target foo2
akrepo --command signtargets

pushd $dest_dir
python3 -m http.server 0&
pid=$!
port=$("$tests_dir/find_listening_port.sh" "$pid")
echo "http server listening on $port"

export OSTREE_SYSROOT=$dest_dir/sysroot
mkdir $OSTREE_SYSROOT
$tests_dir/ostree-scripts/makephysical.sh $OSTREE_SYSROOT

sota_dir=$dest_dir/sota
mkdir $sota_dir
chmod 700 $sota_dir
cat >$sota_dir/sota.toml <<EOF
[uptane]
repo_server = "http://localhost:$port/repo/image"

[provision]
primary_ecu_hardware_id = "hwid-for-test"

[pacman]
type = "ostree"
sysroot = "$OSTREE_SYSROOT"
os = "dummy-os"

[storage]
type = "sqlite"
path = "$sota_dir"
sqldb_path = "sql.db"
uptane_metadata_path = "$sota_dir/metadata"
EOF

## Check that we can do the info command
$valgrind $aklite -h | grep "Command to execute: status, list, update"

## Check that we can do the list command
out=$($valgrind $aklite --loglevel 1 -c $sota_dir/sota.toml list)
if [[ ! "$out" =~ "foo1" ]] ; then
    echo "ERROR: foo1 update missing"
    exit 1
fi
if [[ ! "$out" =~ "foo2" ]] ; then
    echo "ERROR: foo2 update missing"
    exit 1
fi

## Check that we can do the update command
update=$(ostree admin status | head -n 1)
name=$(echo $update | cut -d\  -f1)
sha=$(echo $update | cut -d\  -f2 | sed 's/\.0$//')
echo "Adding new target: $name / $sha"
add_target $name $sha

$valgrind $aklite --loglevel 1 -c $sota_dir/sota.toml update --update-name $name
ostree admin status

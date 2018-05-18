#!/usr/bin/env bash
set -e

BIN_DIR=$1
SOURCE_TESTS_DIR=$2
VALGRIND=${3:-}  # $3 can be set to 'valgrind' (or the run-valgrind wrapper)
ORIG_REPO=$SOURCE_TESTS_DIR/sota_tools/repo

TEST_SRC_REPO=$(mktemp -d)
TEST_DST_REPO=$(mktemp -d)
TEST_REF=master

cp -R $ORIG_REPO/* $TEST_SRC_REPO
ostree show --repo=$TEST_SRC_REPO $TEST_REF > $TEST_SRC_REPO/__ostree_show_$TEST_REF
ostree init --mode=bare --repo=$TEST_DST_REPO

PORT=`$SOURCE_TESTS_DIR/get_open_port.py`

$VALGRIND $BIN_DIR/opcuabridge-ostree-repo-sync-server $TEST_DST_REPO $PORT &
sleep 3
trap 'kill %1' EXIT

$VALGRIND $BIN_DIR/opcuabridge-ostree-repo-sync-client $TEST_SRC_REPO $PORT

ostree show --repo=$TEST_DST_REPO $TEST_REF > $TEST_DST_REPO/__ostree_show_$TEST_REF
diff $TEST_SRC_REPO/__ostree_show_$TEST_REF $TEST_DST_REPO/__ostree_show_$TEST_REF

RET=$?
kill %1

rm -rf $TEST_SRC_REPO
rm -rf $TEST_DST_REPO

exit ${RET}

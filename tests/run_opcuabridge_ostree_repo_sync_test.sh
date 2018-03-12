#!/usr/bin/env bash
set -e

BIN_DIR=$1
SOURCE_TESTS_DIR=$2
ORIG_REPO=$SOURCE_TESTS_DIR/sota_tools/repo

TEST_SRC_REPO=$(mktemp -d)
TEST_DST_REPO=$(mktemp -d)
TEST_REF=master

cp -R $ORIG_REPO/* $TEST_SRC_REPO
ostree show --repo=$TEST_SRC_REPO $TEST_REF > $TEST_SRC_REPO/__ostree_show_$TEST_REF
ostree init --mode=bare --repo=$TEST_DST_REPO

PORT=`$SOURCE_TESTS_DIR/get_open_port.py`

if [ "$3" == "valgrind" ]; then
  valgrind --error-exitcode=1 --leak-check=yes --show-possibly-lost=yes \
           --errors-for-leak-kinds=definite --suppressions=$SOURCE_TESTS_DIR/aktualizr.supp \
           $BIN_DIR/opcuabridge-ostree-repo-sync-server $TEST_DST_REPO $PORT &
else
  $BIN_DIR/opcuabridge-ostree-repo-sync-server $TEST_DST_REPO $PORT &
fi
sleep 3
trap 'kill %1' EXIT

if [ "$3" == "valgrind" ]; then
  $BIN_DIR/run-valgrind $BIN_DIR/opcuabridge-ostree-repo-sync-client $TEST_SRC_REPO $PORT
else
  $BIN_DIR/opcuabridge-ostree-repo-sync-client $TEST_SRC_REPO $PORT
fi

ostree show --repo=$TEST_DST_REPO $TEST_REF > $TEST_DST_REPO/__ostree_show_$TEST_REF
diff $TEST_SRC_REPO/__ostree_show_$TEST_REF $TEST_DST_REPO/__ostree_show_$TEST_REF

RET=$?
kill %1

rm -rf $TEST_SRC_REPO
rm -rf $TEST_DST_REPO

exit ${RET}

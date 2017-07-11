#!/bin/sh

print_usage() {
	echo "test.sh <pass|fail> /path/to/signed.json"
}

PAIR=$(./genpair)
PRIVATE=$(echo ${PAIR} | cut -d : -f1)
PUBLIC=$(echo ${PAIR} | cut -d : -f2)
KEYID=$(echo -n "{\"keytype\":\"ed25519\",\"keyval\":\"${PUBLIC}\"}" | sha256sum | cut -d " " -f1)
if [ "$1" = "pass" ]; then
	SIGNATURE=$(./sign ${PUBLIC} ${PRIVATE} $2)
else
	SIGNATURE=$(xxd -l 64 -c 64 -p /dev/urandom)
fi

DATA=$(cat $2)

TMP=$(mktemp -d /tmp/uptane-XXXXX)
KEYFILE=${TMP}/keys
SIGNEDFILE=${TMP}/signed_data.json

echo "${KEYID}:${PUBLIC}" >${KEYFILE}
echo "{\"signatures\":[{\"keyid\":\"${KEYID}\",\"method\":\"ed25519\",\"sig\":\"${SIGNATURE}\"}],\"signed\":${DATA}}" >${SIGNEDFILE}

echo "./verify_targets ${SIGNEDFILE} ${KEYFILE} 1 0 01:02:03:04:05:06 abc-def ac51637364cff0f6802e087c8c6a51f1925384af639214282a67e093495ef746d8f1c2326cf7182a1067fd69f6049247f509a95a533032f9565c905ce4c5d5fe"
./verify_targets ${SIGNEDFILE} ${KEYFILE} 1 0 01:02:03:04:05:06 abc-def ac51637364cff0f6802e087c8c6a51f1925384af639214282a67e093495ef746d8f1c2326cf7182a1067fd69f6049247f509a95a533032f9565c905ce4c5d5fe
CODE=$?

#rm -r ${TMP}

if [ "$1" = "pass" ]; then
	[ "${CODE}" -eq 0 ]
else
	[ "${CODE}" -ne 0 ]
fi

exit $?

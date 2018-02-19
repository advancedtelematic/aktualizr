#!/usr/bin/env bash
DIR=$(mktemp -d)
mkdir -p ${DIR}

$1 --credentials $2 --device-ca $3 --device-ca-key $4 --local ${DIR}
openssl verify -CAfile $3 ${DIR}/client.pem

rm -r ${DIR}


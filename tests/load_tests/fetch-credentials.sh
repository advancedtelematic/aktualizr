#!/usr/bin/env bash

set -uex

readonly ACCOUNT_ID=${1}
readonly CRYPT_HOST=${2}

accountUrl="http://${CRYPT_HOST}/accounts/${ACCOUNT_ID}"
readonly hostName=$(http --ignore-stdin ${accountUrl} | jq --raw-output .hostName)

credentialsId=$(http --ignore-stdin POST ${accountUrl}/credentials/registration description="load test $(date)" ttl=24 | jq --raw-output .id)

http --ignore-stdin ${accountUrl}/credentials/registration/${credentialsId}  > autoprov_credentials.p12

zip -q prov.zip autoprov_credentials.p12

echo ${hostName}
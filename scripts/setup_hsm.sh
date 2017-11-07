#! /bin/bash

mkdir -p /var/lib/softhsm/tokens
softhsm2-util --init-token --slot 0 --label "Virtual token" --pin 1234 --so-pin 1234
SLOT=`softhsm2-util --show-slots | grep -m 1 -oP 'Slot \K[0-9]+'`
echo "Initialized token in slot: $SLOT"
softhsm2-util --import ./src/tests/test_data/implicit/pkey.pem --label "pkey" --id 02 --slot $SLOT --pin 1234
openssl x509 -outform der -in ./src/tests/test_data/implicit/client.pem -out ./src/tests/test_data/implicit/client.der
pkcs11-tool --module=/usr/lib/softhsm/libsofthsm2.so --id 1 --write-object ./src/tests/test_data/implicit/client.der --type cert --login --pin 1234

openssl pkcs8 -topk8 -inform PEM -outform PEM -nocrypt -in ./src/tests/test_data/priv.key -out ./src/tests/test_data/priv.p8
softhsm2-util --import ./src/tests/test_data/priv.p8 --label "uptane" --id 03 --slot $SLOT --pin 1234

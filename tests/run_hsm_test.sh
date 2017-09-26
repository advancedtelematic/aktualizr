#!/usr/bin/env bash

if [ ! -d /usr/lib/softhsm/softhsm/tokens ]; then
    tar -xf tests/test_data/softhsm.tar.gz -C /var/lib
fi

# if [ -d /usr/lib/softhsm/ ]; then
#  MODULE=/usr/lib/softhsm/libsofthsm2.so
# else
#  MODULE=/usr/lib/x86_64-linux-gnu/softhsm/libsofthsm2.so
# fi

# pkcs11-tool --module=$MODULE --init-token --label "test_token" --so-pin 87654321
# pkcs11-tool --module=$MODULE --init-pin --label "test_token" --so-pin 87654321 --pin 1234
# pkcs11-tool --module=$MODULE  -b -y pubkey --label "test_token"
# pkcs11-tool --module=$MODULE --label "test_token" --write-object tests/test_data/public.key --type pubkey
exec $1
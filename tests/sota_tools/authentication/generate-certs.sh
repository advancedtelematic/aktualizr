#!/bin/bash
set -eEuo pipefail

if [ "$#" -lt 1 ]; then
  echo "Usage: $0 <output directory>"
  exit 1
fi

DEST_DIR="$1"

mkdir -p "$DEST_DIR"
trap 'rm -rf "$DEST_DIR"' ERR
cd $DEST_DIR

cat << 'EOF' > intermediate.ext
subjectKeyIdentifier=hash
authorityKeyIdentifier=keyid,issuer
basicConstraints=critical, CA:TRUE, pathlen:0
keyUsage = critical, digitalSignature, keyCertSign, cRLSign
EOF

cat << 'EOF' > client.ext
subjectKeyIdentifier=hash
authorityKeyIdentifier=keyid,issuer
basicConstraints=CA:FALSE
keyUsage=critical, digitalSignature, nonRepudiation, keyEncipherment
extendedKeyUsage=clientAuth, emailProtection
EOF


# Generate server's key and cert
openssl ecparam -name prime256v1 -genkey -noout -out server.key
openssl req -x509 -new -nodes -key server.key -sha256 -days 3650 -out server.crt -subj "/C=DE/ST=Berlin/L=Berlin/O=Test SERVER CA/OU=ServerCA/CN=localhost"

# Generate client chain
## Root CA
openssl ecparam -name prime256v1 -genkey -noout -out ca.key
openssl req -x509 -new -nodes -key ca.key -sha256 -days 3650 -out ca.crt -subj "/C=DE/ST=Berlin/L=Berlin/O=Test ROOT CA/OU=ROOT CA/CN=localhost"

## Intermediate CA
openssl ecparam -name prime256v1 -genkey -noout -out intermediate.key
openssl req -new -nodes -key intermediate.key -out intermediate.csr -subj "/C=DE/ST=Berlin/L=Berlin/O=Test Intermediate CA/OU=Intermediate CA/CN=localhost"
openssl x509 -req -CA ca.crt -CAkey ca.key -CAcreateserial -extfile intermediate.ext -sha256 -days +3650 -in intermediate.csr -out intermediate.crt

## Client CA
openssl ecparam -name prime256v1 -genkey -noout -out client.key
openssl req -new -nodes -key client.key -out client.csr -subj "/C=DE/ST=Berlin/L=Berlin/O=Test Client/OU=client-namespace/CN=localhost"
openssl x509 -req -CA intermediate.crt -CAkey intermediate.key -CAcreateserial -extfile client.ext -sha256 -days +3650 -in client.csr -out client.crt

## P12 Archive
openssl pkcs12 -export -out client_good.p12 -inkey client.key -in client.crt -certfile intermediate.crt -password pass: -nodes


# Generate rogue chain
## Root CA
openssl ecparam -name prime256v1 -genkey -noout -out rogue_ca.key
openssl req -x509 -new -nodes -key rogue_ca.key -sha256 -days 3650 -out rogue_ca.crt -subj "/C=DE/ST=Berlin/L=Berlin/O=Test ROOT CA/OU=ROOT CA/CN=localhost"

## Intermediate CA
openssl ecparam -name prime256v1 -genkey -noout -out rogue_intermediate.key
openssl req -new -nodes -key rogue_intermediate.key -out rogue_intermediate.csr -subj "/C=DE/ST=Berlin/L=Berlin/O=Test Intermediate CA/OU=Intermediate CA/CN=localhost"
openssl x509 -req -CA rogue_ca.crt -CAkey rogue_ca.key -CAcreateserial -extfile intermediate.ext -sha256 -days +3650 -in rogue_intermediate.csr -out rogue_intermediate.crt

## Client CA
openssl ecparam -name prime256v1 -genkey -noout -out rogue_client.key
openssl req -new -nodes -key rogue_client.key -out rogue_client.csr -subj "/C=DE/ST=Berlin/L=Berlin/O=Test Client/OU=client-namespace/CN=localhost"
openssl x509 -req -CA rogue_intermediate.crt -CAkey rogue_intermediate.key -CAcreateserial -extfile client.ext -sha256 -days +3650 -in rogue_client.csr -out rogue_client.crt

## P12 Archive
openssl pkcs12 -export -out client_bad.p12 -inkey rogue_client.key -in rogue_client.crt -certfile rogue_intermediate.crt -password pass: -nodes

cat << 'EOF' > treehub.json
{
  "ostree": {
    "server": "https://localhost:1443/"
  }
}
EOF

cat << 'EOF' > tufrepo.url
https://localhost:1443/
EOF

cp client_good.p12 client_auth.p12
zip -j good.zip client_auth.p12 treehub.json tufrepo.url

cp client_bad.p12 client_auth.p12
zip -j bad.zip client_auth.p12 treehub.json tufrepo.url

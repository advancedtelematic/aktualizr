#!/bin/bash

repo_dir=$(realpath ${1})
host_addr=${2}
ip_port=9000

gen_certs () {
	certs_dir=${repo_dir}/certs
	mkdir -p ${certs_dir}/client
	mkdir -p ${certs_dir}/server
	cat <<EOF >${certs_dir}/ca.cnf
[req]
req_extensions = cacert
distinguished_name = req_distinguished_name

[req_distinguished_name]

[cacert]
basicConstraints = critical,CA:true
keyUsage = keyCertSign
EOF
        # CA certificate for client devices
        openssl genrsa -out ${certs_dir}/client/ca.private.pem 4096
        openssl req -key ${certs_dir}/client/ca.private.pem -new -x509 -days 7300 -out ${certs_dir}/client/cacert.pem -subj "/C=DE/ST=Berlin/O=Reis und Kichererbsen e.V/commonName=uptane-generator-client-ca" -batch -config ${certs_dir}/ca.cnf -extensions cacert
        # certificate for stand-alone device
        openssl req -out ${certs_dir}/client/standalone_device_cert.csr -subj "/C=DE/ST=Berlin/O=Reis und Kichererbsen e.V/commonName=sota_device" -batch -new -newkey rsa:2048 -nodes -keyout ${certs_dir}/client/standalone_device_key.pem
        openssl x509 -req -in ${certs_dir}/client/standalone_device_cert.csr -CA ${certs_dir}/client/cacert.pem -CAkey ${certs_dir}/client/ca.private.pem -CAcreateserial -out ${certs_dir}/client/standalone_device_cert.pem

        # CA certificate for server
        openssl genrsa -out ${certs_dir}/server/ca.private.pem 4096
        openssl req -key ${certs_dir}/server/ca.private.pem -new -x509 -days 7300 -out ${certs_dir}/server/cacert.pem -subj "/C=DE/ST=Berlin/O=Reis und Kichererbsen e.V/commonName=uptane-generator-server-ca" -batch -config ${certs_dir}/ca.cnf -extensions cacert
        # server's certificate
        openssl req -out ${certs_dir}/server/cert.csr -subj "/C=DE/ST=Berlin/O=Reis und Kichererbsen e.V/commonName=${host_addr}" -batch -new -newkey rsa:2048 -nodes -keyout ${certs_dir}/server/private.pem
        openssl x509 -req -in ${certs_dir}/server/cert.csr -CA ${certs_dir}/server/cacert.pem -CAkey ${certs_dir}/server/ca.private.pem -CAcreateserial -out ${certs_dir}/server/cert.pem

        # bootstrap credentials for device registration. Will not be probably ever used, just to make boostrap process happy
        openssl req -out ${certs_dir}/server/device_bootstrap_cert.csr -subj "/C=DE/ST=Berlin/O=Reis und Kichererbsen e.V/commonName=device_registation" -batch -new -newkey rsa:2048 -nodes -keyout ${certs_dir}/server/device_bootstrap_private.pem
        openssl x509 -req -in ${certs_dir}/server/device_bootstrap_cert.csr -CA ${certs_dir}/server/cacert.pem -CAkey ${certs_dir}/server/ca.private.pem -CAcreateserial -out ${certs_dir}/server/device_bootstrap_cert.pem
}

gen_repo () {
        uptane-generator --command generate --path ${repo_dir}/uptane --keytype ED25519 --expires "3021-07-04T00:00:00Z"
}

gen_ostree () {
        ostree --repo=${repo_dir}/ostree init --mode=archive-z2
}

gen_credentials () {
        TEMPDIR=$(mktemp -d)
        echo -n "https://${host_addr}:${ip_port}" >${TEMPDIR}/autoprov.url
        openssl pkcs12 -export -out ${TEMPDIR}/autoprov_credentials.p12 -in ${repo_dir}/certs/server/device_bootstrap_cert.pem -inkey ${repo_dir}/certs/server/device_bootstrap_private.pem -CAfile ${repo_dir}/certs/server/cacert.pem -chain -password pass: -passin pass:
        CURDIR=$(pwd)
        cd "$TEMPDIR"
        zip ${repo_dir}/credentials.zip ./*
        cd "$CURDIR"
        rm -r ${TEMPDIR}
}

gen_site_conf () {
        cat <<EOF >${repo_dir}/site.conf
OSTREE_REPO = "${repo_dir}/ostree"
SOTA_PACKED_CREDENTIALS = "${repo_dir}/credentials.zip"
SOTA_CLIENT_PROV = "aktualizr-device-prov"
SOTA_CACERT_PATH = "${repo_dir}/certs/client/cacert.pem"
SOTA_CAKEY_PATH = "${repo_dir}/certs/client/ca.private.pem"
EOF
}

gen_local_toml () {
        mkdir -p ${repo_dir}/var_sota
        echo -n "https://${host_addr}:${ip_port}" >${repo_dir}/var_sota/gateway.url
        chmod 744 ${repo_dir}/var_sota
        cat << EOF >${repo_dir}/sota.toml
[tls]
server_url_path = "${repo_dir}/var_sota/gateway.url"

[provision]
provision_path = "${repo_dir}/credentials.zip"
primary_ecu_hardware_id = "desktop"

[storage]
type = "sqlite"
path = "${repo_dir}/var_sota"
sqldb_path = "${repo_dir}/var_sota/sql.db"

[pacman]
type = "none"

[import]
tls_cacert_path = "${repo_dir}/certs/server/cacert.pem"
tls_clientcert_path = "${certs_dir}/client/standalone_device_cert.pem"
tls_pkey_path = "${certs_dir}/client/standalone_device_key.pem"
EOF
}

if [ -e ${repo_dir} ]; then
        echo "File or directory ${1} already exists"
        exit -1
fi

rm -rf "$repo_dir"
gen_certs
gen_repo
gen_ostree
gen_credentials
gen_site_conf
gen_local_toml

#!/usr/bin/env bash

get_variable() {
	sed -ne "/\[import\]/,/\[.*\]/s/$1\s*=\s*\"\(.*\)\".*\$/\1/p" < $2
}

for f in $1/*.toml; do
	if [[ $( get_variable tls_cacert_path $f ) = "/var/sota/root.crt" ]]; then
		echo "import.tls_cacert_path in $f is the same as path for FS->SQL migration (/var/sota/root.crt)" >&2
		exit 1;
	fi

	if [[ $(get_variable tls_pkey_path $f) = "/var/sota/pkey.pem" ]]; then
		echo "import.tls_pkey_path in $f is the same as path for FS->SQL migration (/var/sota/pkey)" >&2
		exit 1;
	fi
	
	if [[ $(get_variable tls_clientcert_path $f) = "/var/sota/client.pem" ]]; then
		echo "import.tls_clientcert_path in $f is the same as path for FS->SQL migration (/var/sota/client.pem)" >&2
		exit 1;
	fi
done

exit 0

#!/usr/bin/env python3

import argparse
import OpenSSL
import os
import re
import subprocess
import sys
import zipfile


def main(credentials, config_input, output_dir, working_dir):
    config_output = output_dir + '/sota.toml'
    ret = subprocess.call([working_dir + '/aktualizr_implicit_writer', '-c', credentials, '-i', config_input, '-o', config_output, '-p', output_dir])
    if ret != 0:
        print('failed:' + str(ret))
        sys.exit(ret)
    conf_file = open(config_output, 'rb')

    zip_ref = zipfile.ZipFile(credentials, 'r')
    p12_file = zip_ref.open('autoprov_credentials.p12', mode='r')
    url_file = zip_ref.open('autoprov.url', mode='r')
    zip_ref.close()

    # Test server URL consistency:
    server = ''
    tls = False
    for line in conf_file:
        print('LINE:' + str(line))
        if re.search(rb'tls_cacert_path\s*=', line):
            root_ca_path = line.split(b'=')[1].strip()[1:-1]
        elif re.search(rb'^\[tls\]$', line):
            tls = True
        elif re.search(rb'^\[', line):
            tls = False
        elif tls and re.search(rb'server\s*=', line):
            server = line.split(b'=')[1].strip()[1:-1]
    url = url_file.read()
    if url != server:
        print('Server URL does not match.')
        sys.exit(1)

    # Test root CA consistency:
    root_ca_file = open(output_dir + root_ca_path.decode(), 'rb')
    root_ca = root_ca_file.read()
    p12_data = OpenSSL.crypto.load_pkcs12(p12_file.read())
    ca = p12_data.get_ca_certificates()
    ca_str = b''
    for cert in ca:
        ca_str = ca_str + OpenSSL.crypto.dump_certificate(OpenSSL.crypto.FILETYPE_PEM, cert)
    if ca_str != root_ca:
        print('Root CA does not match.')
        sys.exit(1)

    print('Success!')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(os.path.basename(__file__))
    parser.add_argument('-c', '--credentials', help='zipped credentials file',
                        required=True)
    parser.add_argument('-i', '--config-input', help='input sota.toml configuration file', required=True)
    parser.add_argument('-o', '--output-dir', help='output directory', default='.')
    parser.add_argument('-w', '--working-dir', help='working directory', default='.')
    args = parser.parse_args()

    main(args.credentials, args.config_input, args.output_dir, args.working_dir)

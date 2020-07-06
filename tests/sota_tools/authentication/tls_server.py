#!/usr/bin/python3
from http.server import HTTPServer,SimpleHTTPRequestHandler
import argparse
import os.path
import socket
import ssl

class ReUseHTTPServer(HTTPServer):
    def server_bind(self):
        self.socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        HTTPServer.server_bind(self)

parser = argparse.ArgumentParser()
parser.add_argument('port', type=int)
parser.add_argument('cert_path')
parser.add_argument('--noauth', action='store_true')
args = parser.parse_args()

httpd = ReUseHTTPServer(('localhost', args.port), SimpleHTTPRequestHandler)
httpd.socket = ssl.wrap_socket (httpd.socket,
                                certfile=os.path.join(args.cert_path, 'server.crt'),
                                keyfile=os.path.join(args.cert_path, 'server.key'),
                                server_side=True,
                                cert_reqs = ssl.CERT_NONE if args.noauth else ssl.CERT_REQUIRED,
                                ca_certs = os.path.join(args.cert_path, 'ca.crt'))
httpd.serve_forever()

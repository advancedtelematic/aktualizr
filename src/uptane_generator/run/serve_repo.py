#!/usr/bin/python3

import sys
import os
import socket
from http.server import BaseHTTPRequestHandler, HTTPServer
import ssl
import json

top_path = sys.argv[2]


class Handler(BaseHTTPRequestHandler):
    def do_GET(self):
        print("GET: " + self.path)
        if self.path.startswith('/director/'):
            self.do_get_static(top_path + "/uptane/repo/director/", self.path[10:])
        elif self.path.startswith('/repo/'):
            self.do_get_static(top_path + "/uptane/repo/repo/", self.path[6:])
        elif self.path.startswith('/treehub/'):
            self.do_get_static(top_path + "/uptane/repo/ostree/", self.path[9:])
        else:
            self.send_response(404)
            self.end_headers()

    def do_get_static(self, dir_path, path):
        path = os.path.join(dir_path, path)
        if os.path.isfile(path):
            self.send_response(200)
            self.end_headers()
            with open(path, "rb") as f:
                self.wfile.write(f.read())
        else:
            self.send_response(404)
            self.end_headers()

    def do_POST(self):
        length = int(self.headers.get('content-length'))
        data = json.loads(self.rfile.read(length).decode('utf-8'))
        if self.path == "/director/ecus":
            return self.do_ecuRegister(data)
        elif self.path == "/director/manifest":
            return self.do_manifest(data)
        else:
            self.send_response(404)
            self.end_headers()

    def do_ecuRegister(self, data):
        self.send_response(200)
        self.end_headers()
        self.wfile.write(b"{}")

    def do_manifest(self, data):
        self.send_response(200)
        self.end_headers()
        self.wfile.write(b"{}")

    def do_PUT(self):
        self.do_POST()


class ReUseHTTPServer(HTTPServer):
    def server_bind(self):
        self.socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        HTTPServer.server_bind(self)


server_address = ('', int(sys.argv[1]))
httpd = ReUseHTTPServer(server_address, Handler)

context = ssl.SSLContext(ssl.PROTOCOL_TLS if hasattr(ssl, 'PROTOCOL_TLS') else ssl.PROTOCOL_TLSv1_1)
context.load_cert_chain(certfile=top_path + '/certs/server/cert.pem',
                        keyfile=top_path + '/certs/server/private.pem')
context.load_verify_locations(cafile=top_path + '/certs/client/cacert.pem')
context.verify_mode = ssl.CERT_REQUIRED
httpd.socket = context.wrap_socket(httpd.socket, server_side=True)
httpd.serve_forever()

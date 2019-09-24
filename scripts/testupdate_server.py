#!/usr/bin/python3

import sys
import socket
from http.server import BaseHTTPRequestHandler, HTTPServer


class Handler(BaseHTTPRequestHandler):
    def do_GET(self):
        local_path = self.path
        print("GET: " + local_path)
        self.send_response(200)
        self.end_headers()
        with open(self.server.base_dir + "/fake_root/repo/" + local_path, "rb") as fl:
            self.wfile.write(bytearray(fl.read()))

    def do_POST(self):
        self.send_response(200)
        self.end_headers()

    def do_PUT(self):
        self.send_response(200)
        self.end_headers()


class SelfUpdateHTTPServer(HTTPServer):
    def __init__(self, base_dir, *kargs, **kwargs):
        super().__init__(*kargs, **kwargs)
        self.base_dir = base_dir

    def server_bind(self):
        self.socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        HTTPServer.server_bind(self)


server_address = ('', int(sys.argv[1]))
base_dir = sys.argv[2]
httpd = SelfUpdateHTTPServer(base_dir, server_address, Handler)

try:
    httpd.serve_forever()
except KeyboardInterrupt:
    print("fake_uptane_server.py exiting...")

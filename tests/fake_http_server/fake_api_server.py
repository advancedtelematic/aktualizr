#!/usr/bin/python3

import sys
import os
import socket
import json
from http.server import BaseHTTPRequestHandler, HTTPServer

class Handler(BaseHTTPRequestHandler):
    def do_GET(self):
        self.send_response(200)
        self.end_headers()
        if self.path == "/api/v1/user_repo/targets.json":
            with open(os.path.join(sys.argv[2], "repo/repo/targets.json")) as f:
                self.wfile.write(bytes(f.read(), "utf8"))

    def do_POST(self):
        if self.path == "/token":
            token = b'{"access_token":"fake_token"}'
            self.send_response(200)
            self.end_headers()
            self.wfile.write(token)
    def do_HEAD(self):
        self.send_response(200)
        self.end_headers()


class ReUseHTTPServer(HTTPServer):
    def server_bind(self):
        self.socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        HTTPServer.server_bind(self)


server_address = ('', int(sys.argv[1]))
httpd = ReUseHTTPServer(server_address, Handler)

try:
    httpd.serve_forever()
except KeyboardInterrupt as k:
    print("fake_api_server.py exiting...")
    pass


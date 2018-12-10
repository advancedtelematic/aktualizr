#!/usr/bin/python3

import sys
import socket
from http.server import BaseHTTPRequestHandler, HTTPServer


class Handler(BaseHTTPRequestHandler):
    def do_GET(self):
        print("GET: " + self.path)
        self.send_response(200)
        self.end_headers()
        with open("/persistent/fake_root/repo/" + self.path, "rb") as fl:
            self.wfile.write(bytearray(fl.read()))

    def do_POST(self):
        self.send_response(200)
        self.end_headers()

    def do_PUT(self):
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
except KeyboardInterrupt:
    print("fake_uptane_server.py exiting...")

#!/usr/bin/python3

import sys
import codecs
import socket
import ssl
import os
from http.server import BaseHTTPRequestHandler, HTTPServer
from json import dump
from tempfile import NamedTemporaryFile

made_requests = 0

class TreehubServerHandler(BaseHTTPRequestHandler):
    def __init__(self, *args):
        BaseHTTPRequestHandler.__init__(self, *args)

    def do_GET(self):
        if self.drop_check():
            return
        path = os.path.join(repo_path, self.path[1:])
        if(os.path.exists(path)):
            self.send_response_only(200)
            self.end_headers()
            with open(path, 'rb') as source:
                while True:
                    data = source.read(1000)
                    if not data:
                        break
                    self.wfile.write(data)
        else:
            self.send_response_only(404)
            self.end_headers()
    def do_HEAD(self):
        if self.drop_check():
            return
        path = os.path.join(repo_path, self.path[1:])
        if(os.path.exists(path)):
            self.send_response_only(200)
        else:
            self.send_response_only(404)
        self.end_headers()
    def drop_check(self):
        global made_requests
        print("made requests: %d" % made_requests)
        if drop_connection_every_n_request and made_requests + 1 == drop_connection_every_n_request:
            made_requests = 0
            print("Dropping this connection")
            return True
        else:
            made_requests += 1
            return False


class ReUseHTTPServer(HTTPServer):
    def server_bind(self):
        self.socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        HTTPServer.server_bind(self)


server_address = ('', int(sys.argv[1]))
httpd = ReUseHTTPServer(server_address, TreehubServerHandler)
repo_path = "tests/sota_tools/repo"
if len(sys.argv) == 3 and sys.argv[2]:
    print("Dropping connection after every: %s request" % sys.argv[2])
    drop_connection_every_n_request = int(sys.argv[2])
else:
    drop_connection_every_n_request = 0

try:
    httpd.serve_forever()
except KeyboardInterrupt as k:
    print("%s exiting..." % sys.argv[0])


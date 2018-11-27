#!/usr/bin/python3

import sys
import codecs
import socket
import ssl
import os
import time
from http.server import BaseHTTPRequestHandler, HTTPServer
from json import dump
from tempfile import NamedTemporaryFile

class TreehubServerHandler(BaseHTTPRequestHandler):
    made_requests = {}

    def __init__(self, *args):
        BaseHTTPRequestHandler.__init__(self, *args)

    def do_GET(self):
        if self.drop_check():
            print("Dropping GET request %s" % self.path)
            return
        print("Processing GET request %s" % self.path)
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
            if path.endswith("ef2f2629dc9263fdf3c0f032563a2d757623bbc11cf99df25c3c3f258dccbe.commitmeta"):
                time.sleep(1.5) # slow down download to be sure that progress callback will be called
            self.send_response_only(404)
            self.end_headers()

    def do_HEAD(self):
        print("GOT HEAD request")
        if self.drop_check():
            print("Dropping HEAD request %s" % self.path)
            return
        print("Processing HEAD request %s" % self.path)
        path = os.path.join(repo_path, self.path[1:])
        if(os.path.exists(path)):
            self.send_response_only(200)
        else:
            self.send_response_only(404)
        self.end_headers()

    def drop_check(self):
        if drop_connection_every_n_request == 0:
            return False
        self.made_requests[self.path] = self.made_requests.get(self.path, 0) + 1
        print("request number: %d" % self.made_requests[self.path])
        if self.made_requests[self.path] == 1:
            return True
        else:
            if drop_connection_every_n_request == self.made_requests[self.path]:
                self.made_requests[self.path] = 0
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


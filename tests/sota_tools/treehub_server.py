#!/usr/bin/python3

import sys
import codecs
import socket
import ssl
import os
from http.server import BaseHTTPRequestHandler, HTTPServer
from json import dump
from tempfile import NamedTemporaryFile

class TreehubServerHandler(BaseHTTPRequestHandler):
    def __init__(self, *args):
        BaseHTTPRequestHandler.__init__(self, *args)

    def do_GET(self):
        
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
        path = os.path.join(repo_path, self.path[1:])
        if(os.path.exists(path)):
            self.send_response_only(200)
        else:
            self.send_response_only(404)
        self.end_headers()
        
    def send_text(self, text):
        self.send_response_only(200)
        self.send_header('Content-type', 'text/text')
        self.end_headers()
        self.wfile.write(text)


class ReUseHTTPServer(HTTPServer):
    def server_bind(self):
        self.socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        HTTPServer.server_bind(self)

server_address = ('', int(sys.argv[1]))
httpd = ReUseHTTPServer(server_address, TreehubServerHandler)
repo_path = sys.argv[2]

try:
    httpd.serve_forever()
except KeyboardInterrupt as k:
    print("%s exiting..." % sys.argv[0])


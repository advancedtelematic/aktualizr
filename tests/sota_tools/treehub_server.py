#!/usr/bin/python3

import sys
import codecs
import socket
import ssl
from http.server import BaseHTTPRequestHandler, HTTPServer
from json import dump
from tempfile import NamedTemporaryFile


class TreehubServerHandler(BaseHTTPRequestHandler):
    def __init__(self, *args):
        BaseHTTPRequestHandler.__init__(self, *args)

    def do_GET(self):
        
        if self.path.endswith("/config"):
            config = b"[core]\nrepo_version=1\nmode=archive-z2"
            self.send_text(config)
        elif self.path.endswith("refs/heads/master"):
            self.send_text(b"16ef2f2629dc9263fdf3c0f032563a2d757623bbc11cf99df25c3c3f258dccbe")
        elif self.path.endswith("/objects/2a/28dac42b76c2015ee3c41cc4183bb8b5c790fd21fa5cfa0802c6e11fd0edbe.filez"):
            self.send_text(b"good")
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

try:
    httpd.serve_forever()
except KeyboardInterrupt as k:
    print("%s exiting..." % sys.argv[0])


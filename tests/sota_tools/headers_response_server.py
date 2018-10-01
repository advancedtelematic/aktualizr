#!/usr/bin/python3

import sys
import codecs
import socket
from http.server import BaseHTTPRequestHandler, HTTPServer
from json import dump
from tempfile import NamedTemporaryFile


class HeadersServerHandler(BaseHTTPRequestHandler):
    def __init__(self, *args):
        BaseHTTPRequestHandler.__init__(self, *args)

    def do_GET(self):
        self.send_response_only(200)
        self.send_header('Content-type', 'text/json')
        self.end_headers()
        writer = codecs.getwriter('UTF8')(self.wfile)
        dump(dict(self.headers), writer)
        writer.flush()


class ReUseHTTPServer(HTTPServer):
    def server_bind(self):
        self.socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        HTTPServer.server_bind(self)

server_address = ('', int(sys.argv[1]))
httpd = ReUseHTTPServer(server_address, HeadersServerHandler)

try:
    httpd.serve_forever()
except KeyboardInterrupt as k:
    print("%s exiting..." % sys.argv[0])

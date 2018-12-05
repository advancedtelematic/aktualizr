#!/usr/bin/python3

import sys
import socket
from http.server import SimpleHTTPRequestHandler, HTTPServer
from time import sleep

last_fails = False

class Handler(SimpleHTTPRequestHandler):
    def do_GET(self):
        global last_fails
        if self.path == '/download':
            self.send_response(301)
            self.send_header('Location', '/download/file')
            self.end_headers()
        elif self.path == '/download/file':
            self.send_response(200)
            self.end_headers()
            self.wfile.write(b'content')
        elif self.path == '/auth_call':
            self.send_response(200)
            self.end_headers()
            if 'Authorization' in self.headers:
                auth_list = self.headers['Authorization'].split(' ')
                if auth_list[0] == 'Bearer' and auth_list[1] == 'token':
                    self.wfile.write(b'{"status": "good"}')
            self.wfile.write(b'{}')
        elif self.path.endswith('/large_file') or self.path.endswith('/large_interrupted'):
            response_size = 2048
            if "Range" in self.headers:
                r = self.headers["Range"]
                r_from = int(r.split("=")[1].split("-")[0])
                self.send_response(206)
                self.send_header('Content-Range', 'bytes %d-%d/%d' %(r_from, response_size-1, response_size))
                response_size = response_size - r_from
            else:
                if self.path.endswith('/large_interrupted'):
                    response_size = 1024
                self.send_response(200)
            self.send_header('Content-Length', response_size)
            self.end_headers()
            sleep(0.5)
            for i in range(response_size):
              self.wfile.write(b'@')
        elif self.path == '/slow_file':
            self.send_response(200)
            self.end_headers()
            for i in range(5):
              self.wfile.write(b'aa')
              sleep(1)
        else:
            if not last_fails:
                self.send_response(503)
                self.end_headers()
                last_fails = True
                self.wfile.write(b"Internal server error")
                return
            else:
                last_fails = False

            self.send_response(200)
            self.end_headers()
            self.wfile.write(b'{"path": "%b"}'%bytes(self.path, "utf8"))

    def do_POST(self):
        global last_fails
        if not last_fails:
            self.send_response(503)
            self.end_headers()
            last_fails = True
            self.wfile.write(b"Internal server error")
            return
        else:
            last_fails = False
        if self.path == '/token':
            self.send_response(200)
            self.end_headers()
            if 'Authorization' in self.headers:
                self.wfile.write(b"{\"access_token\": \"token\"}")
            else:
                self.wfile.write(b'')

        else:
            self.send_response(200)
            self.end_headers()
            length = int(self.headers.get('content-length'))
            result = b'{"data": %b, "path": "%b"}'%(self.rfile.read(length), bytes(self.path, "utf8"))
            self.wfile.write(result)

    def do_PUT(self):
        self.do_POST()


class ReUseHTTPServer(HTTPServer):
    def server_bind(self):
        self.socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        HTTPServer.server_bind(self)


server_address = ('', int(sys.argv[1]))
httpd = ReUseHTTPServer(server_address, Handler)
httpd.serve_forever()


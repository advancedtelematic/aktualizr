#!/usr/bin/python3

import sys
import socket
from http.server import SimpleHTTPRequestHandler, HTTPServer
from time import sleep

class Handler(SimpleHTTPRequestHandler):
    def do_GET(self):

        if self.path.startswith("/director/") and self.path.endswith(".json"):
            self.send_response(200)
            self.end_headers()
            role = self.path[len("/director/"):]
            with open(meta_path + '/repo/director/' + role, 'rb') as source:
                while True:
                    data = source.read(1024)
                    if not data:
                        break
                    self.wfile.write(data)
            return
        elif self.path.startswith("/repo/") and self.path.endswith(".json"):
            self.send_response(200)
            self.end_headers()
            role = self.path[len("/repo/"):]
            with open(meta_path + '/repo/image/' + role, 'rb') as source:
                while True:
                    data = source.read(1024)
                    if not data:
                        break
                    self.wfile.write(data)
            return
        elif self.path.startswith("/repo/targets"):
            self.send_response(200)
            self.end_headers()
            filename = self.path[len("/repo/targets"):]
            with open(file_path + filename, 'rb') as source:
                while True:
                    data = source.read(1024)
                    if not data:
                        break
                    self.wfile.write(data)
            return

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
        elif self.path.endswith('/large_file'):
            chunk_size = 1 << 20
            response_size = 100 * chunk_size
            if "Range" in self.headers:
                r = self.headers["Range"]
                r_from = int(r.split("=")[1].split("-")[0])
                self.send_response(206)
                self.send_header('Content-Range', 'bytes %d-%d/%d' %(r_from, response_size-1, response_size))
                response_size = response_size - r_from
            else:
                self.send_response(200)
            self.send_header('Content-Type', 'application/json')
            self.send_header('Content-Length', response_size)
            self.end_headers()
            num_chunks, last_chunk = divmod(response_size, chunk_size)
            b = b'@' * chunk_size
            try:
                while num_chunks > 0:
                    self.wfile.write(b)
                    num_chunks -= 1
                self.wfile.write(b'@' * last_chunk)
            except ConnectionResetError:
                return
        elif self.path == '/slow_file':
            self.send_response(200)
            self.end_headers()
            for i in range(5):
              self.wfile.write(b'aa')
              sleep(1)
        else:
            self.send_response(200)
            self.end_headers()
            self.wfile.write(b'{"path": "%b"}'%bytes(self.path, "utf8"))

    def do_POST(self):
        if self.path == '/devices':
            self.send_response(200)
            self.end_headers()
            with open('tests/test_data/cred.p12', 'rb') as source:
                while True:
                    data = source.read(1024)
                    if not data:
                        break
                    self.wfile.write(data)

        elif self.path == "/director/ecus":
            self.send_response(200)
            self.end_headers()
            self.wfile.write(b"{}")
            return

        elif self.path == "/director/manifest":
            self.send_response(200)
            self.end_headers()
            self.wfile.write(b"{}")
            return

        elif self.path == '/token':
            self.send_response(200)
            self.end_headers()
            if 'Authorization' in self.headers:
                self.wfile.write(b"{\"access_token\": \"token\"}")
            else:
                self.wfile.write(b'')

        else:
            self.send_response(404)
            self.end_headers()

    def do_PUT(self):
        self.do_POST()


class ReUseHTTPServer(HTTPServer):
    def server_bind(self):
        self.socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        HTTPServer.server_bind(self)


server_address = ('', int(sys.argv[1]))
file_path = sys.argv[2]
meta_path = sys.argv[3]

httpd = ReUseHTTPServer(server_address, Handler)
try:
    httpd.serve_forever()
except KeyboardInterrupt:
    httpd.server_close()


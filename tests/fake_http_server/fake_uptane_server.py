#!/usr/bin/python3

import sys
import socket
import json
from http.server import BaseHTTPRequestHandler, HTTPServer


class Handler(BaseHTTPRequestHandler):
    def do_GET(self):
        if self.path.startswith("/repo/targets/download_failure"):
            if "Range" in self.headers: #restored connection
                r = self.headers["Range"]
                r_from = int(r.split("=")[1].split("-")[0])
                if r_from == 1024:
                    self.send_response(206)
                    self.send_header('Content-Range', 'bytes %d-%d/%d' %(r_from, 2047, 2048))
                    self.send_header('Content-Length', 1024)
                else:
                    self.send_response(503) #Error, we should ask to restore from 1024 byte
                    self.end_headers()
                    return
            else: # First time drop connection
                self.send_response(200)
                self.send_header('Content-Length', 2048)
            self.end_headers()
            for i in range(1024):
              self.wfile.write(b'@')

    def do_POST(self):
        length = int(self.headers.get('content-length'))
        data = json.loads(self.rfile.read(length).decode('utf-8'))
        if self.path == "/devices":
            return self.do_devicesRegister(data)
        elif self.path == "/director/ecus":
            return self.do_ecuRegister(data)
        else:
            self.send_response(404)
            self.end_headers()

    def do_devicesRegister(self, data):
        if data["ttl"] == "drop_request":
            return
        elif data["ttl"] == "drop_body":
            self.send_response(200)
            self.end_headers()
            self.wfile.write(b"some partial response")
        elif data["ttl"].startswith("status_"):
            self.send_response(int(data["ttl"][7:]))
            self.end_headers()
        elif data["ttl"] == "noerrors" or data["ttl"] == "download_failure":
            self.send_response(200)
            self.end_headers()
            f = open('tests/test_data/cred.p12', 'rb')
            self.wfile.write(f.read())
        else:
            self.send_response(404)
            self.end_headers()

    def do_ecuRegister(self, data):
        if data["primary_ecu_serial"] == "drop_request":
            return
        elif data["primary_ecu_serial"].startswith("status_"):
            self.send_response(int(data["primary_ecu_serial"][7:]))
            self.end_headers()
        elif data["primary_ecu_serial"] == "noerrors" or data["primary_ecu_serial"] == "download_failure":
            self.send_response(200)
            self.end_headers()
            self.wfile.write(b"{}")
        else:
            self.send_response(404)
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
    print("fake_uptane_server.py exiting...")
    pass


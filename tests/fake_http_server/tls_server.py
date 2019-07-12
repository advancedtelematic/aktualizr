#!/usr/bin/python3
from http.server import HTTPServer,SimpleHTTPRequestHandler
import socket
import ssl

class ReUseHTTPServer(HTTPServer):
    def server_bind(self):
        self.socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        HTTPServer.server_bind(self)

httpd = ReUseHTTPServer(('localhost', 1443), SimpleHTTPRequestHandler)
httpd.socket = ssl.wrap_socket (httpd.socket,
                                certfile='tests/fake_http_server/server.crt',
                                keyfile='tests/fake_http_server/server.key',
                                server_side=True,
                                cert_reqs = ssl.CERT_REQUIRED,
                                ca_certs = "tests/fake_http_server/ca.crt")
httpd.serve_forever()

#!/usr/bin/python3
from http.server import HTTPServer,SimpleHTTPRequestHandler
import ssl

httpd = HTTPServer(('localhost', 1443), SimpleHTTPRequestHandler)
httpd.socket = ssl.wrap_socket (httpd.socket, certfile='fake_http_server/client.crt', keyfile='fake_http_server/client.key',
    server_side=True, cert_reqs = ssl.CERT_REQUIRED, ca_certs = "fake_http_server/client.crt")
httpd.serve_forever()

#!/usr/bin/python3
import sys
import socket
import json
from http.server import BaseHTTPRequestHandler, HTTPServer


class Handler(BaseHTTPRequestHandler):
	def do_GET(self):
		pass


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
			self.wfile.write("some partial response")
		elif data["ttl"].startswith("status_"):
			self.send_response(int(data["ttl"][7:]))
			self.end_headers()
		elif data["ttl"] == "noerrors":
			self.send_response(200)
			self.end_headers()
			f = open('tests/test_data/cred.p12', 'rb')
			self.wfile.write(f.read())

	def do_ecuRegister(self, data):
		if data["primary_ecu_serial"] == "drop_request":
			return
		elif data["primary_ecu_serial"].startswith("status_"):
			self.send_response(int(data["primary_ecu_serial"][7:]))
			self.end_headers()
		elif data["primary_ecu_serial"] == "noerrors":
			self.send_response(200)
			self.end_headers()
			self.wfile.write("{}")



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


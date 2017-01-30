#!/usr/bin/python

from BaseHTTPServer import BaseHTTPRequestHandler, HTTPServer

class Handler(BaseHTTPRequestHandler):
	def do_GET(self):
		if self.path == '/download':
			self.send_response(301)
			self.send_header('Location', '/download/file')
			self.end_headers()
		elif self.path == '/download/file':
			self.send_response(200)
			self.end_headers()
			self.wfile.write('content')
		elif self.path == '/auth_call':
			self.send_response(200)
			self.end_headers()
			if 'Authorization' in self.headers:
				auth_list = self.headers['Authorization'].split(' ')
				if auth_list[0] == 'Bearer' and auth_list[1] == 'token':
					self.wfile.write('{"status": "good"}')
			self.wfile.write('{}')
		else:
			self.send_response(200)
			self.end_headers()
			self.wfile.write('{"path": "%s"}'%self.path)
	
	def do_POST(self):
		if self.path == '/token':
			self.send_response(200)
			self.end_headers()
			if 'Authorization' in self.headers:
				self.wfile.write("{\"access_token\": \"token\"}")
			else:
				self.wfile.write('')
					
		else:
			self.send_response(200)
			self.end_headers()
			length = int(self.headers.getheader('content-length'))
			result = '{"data": %s, "path": "%s"}'%(self.rfile.read(length), self.path)
			self.wfile.write(result)


server_address = ('', 8800)
httpd = HTTPServer(server_address, Handler)
httpd.serve_forever()


#!/usr/bin/python3

import argparse
import contextlib
import multiprocessing
import logging
import os
import sys
import socket
import socketserver

from http.server import SimpleHTTPRequestHandler, HTTPServer
from os import path
from time import sleep


logger = logging.getLogger("fake_test_server")


class FailInjector:
    def __init__(self):
        self.last_fails = False

    def fail(self, http_handler):
        if self.last_fails:
            self.last_fails = False
            return False
        else:
            http_handler.send_response(503)
            http_handler.end_headers()
            http_handler.wfile.write(b"Internal server error")
            self.last_fails = True
            return True


class Handler(SimpleHTTPRequestHandler):
    def _serve_simple(self, uri):
        with open(uri, 'rb') as source:
            while True:
                data = source.read(1024)
                if not data:
                    break
                self.wfile.write(data)

    def serve_meta(self, uri):
        if self.server.meta_path is None:
            raise RuntimeError("Please supply a path for metadata")
        self._serve_simple(self.server.meta_path + uri)

    def serve_target(self, filename):
        if self.server.target_path is None:
            raise RuntimeError("Please supply a path for targets")
        self._serve_simple(self.server.target_path + filename)

    def do_GET(self):
        if self.path.startswith("/director/") and self.path.endswith(".json"):
            self.send_response(200)
            self.end_headers()
            role = self.path[len("/director/"):]
            self.serve_meta("/repo/director/" + role)
        elif self.path.startswith("/repo/") and self.path.endswith(".json"):
            self.send_response(200)
            self.end_headers()
            role = self.path[len("/repo/"):]
            self.serve_meta('/repo/repo/' + role)
        elif self.path.startswith("/repo/targets"):
            self.send_response(200)
            self.end_headers()
            filename = self.path[len("/repo/targets"):]
            self.serve_target(filename)

        elif self.path == '/download':
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
                self.send_header('Content-Range', 'bytes %d-%d/%d' % (r_from, response_size-1, response_size))
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
        elif self.path == '/campaigner/campaigns':
            self.send_response(200)
            self.end_headers()
            self.serve_meta("/campaigns.json")
        elif self.path == '/user_agent':
            user_agent = self.headers.get('user-agent')
            self.send_response(200)
            self.end_headers()
            self.wfile.write(user_agent.encode())
        else:
            if self.server.fail_injector is not None and self.server.fail_injector.fail(self):
                return
            self.send_response(200)
            self.end_headers()
            self.wfile.write(b'{"path": "%b"}' % bytes(self.path, "utf8"))

    def do_POST(self):
        if self.server.fail_injector is not None and self.server.fail_injector.fail(self):
            return

        if self.path == '/devices':
            self.send_response(200)
            self.end_headers()
            with open(path.join(self.server.srcdir, 'tests/test_data/cred.p12'), 'rb') as source:
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
            content_length = int(self.headers['Content-Length'])  # <--- Gets the size of data
            post_data = self.rfile.read(content_length)  # <--- Gets the data itself
            print(post_data)  # <-- Print post data
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
            # for httpclient_test
            self.send_response(200)
            self.end_headers()
            length = int(self.headers.get('content-length'))
            result = b'{"data": %b, "path": "%b"}'%(self.rfile.read(length), bytes(self.path, "utf8"))
            self.wfile.write(result)

    def do_PUT(self):
        self.do_POST()


class FakeTestServer(socketserver.ThreadingMixIn, HTTPServer):
    def __init__(self, addr, meta_path, target_path, srcdir=None, fail_injector=None):
        super(HTTPServer, self).__init__(server_address=addr, RequestHandlerClass=Handler)
        self.meta_path = meta_path
        if target_path is not None:
            self.target_path = target_path
        elif meta_path is not None:
            self.target_path = path.join(meta_path, 'repo/repo/targets')
        else:
            self.target_path = None
        self.fail_injector = fail_injector
        self.srcdir = srcdir if srcdir is not None else os.getcwd()

    def server_bind(self):
        self.socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        HTTPServer.server_bind(self)


class FakeTestServerBackground:

    def __init__(self, meta_path, target_path=None, srcdir=None, port=0):
        if srcdir is None:
            srcdir = os.getcwd()
        self._httpd = FakeTestServer(addr=('', port), meta_path=meta_path,
                                     target_path=target_path, srcdir=srcdir)
        self._server_process = self.__class__.Process(target=self._httpd.serve_forever)
        self.base_url = 'http://localhost:{}'.format(self.port)

    class Process(multiprocessing.Process):
        def run(self):
            with open(os.devnull, 'w') as devnull_fd:
                with contextlib.redirect_stdout(devnull_fd),\
                        contextlib.redirect_stderr(devnull_fd):
                    super(multiprocessing.Process, self).run()

    @property
    def port(self):
        return self._httpd.server_port

    def __enter__(self):
        self._server_process.start()
        logger.debug("Uptane server running and listening: {}".format(self.port))
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self._server_process.terminate()
        self._server_process.join(timeout=10)
        logger.debug("Uptane server has been stopped")


def main():
    parser = argparse.ArgumentParser(description='Run a fake OTA backend')
    parser.add_argument('port', type=int, help='server port')
    parser.add_argument('-t', '--targets', help='targets directory', default=None)
    parser.add_argument('-m', '--meta', help='meta directory', default=None)
    parser.add_argument('-f', '--fail', help='enable intermittent failure', action='store_true')
    parser.add_argument('-s', '--srcdir', help='path to the aktualizr source directory')
    args = parser.parse_args()

    httpd = FakeTestServer(('', args.port), meta_path=args.meta,
                           target_path=args.targets, srcdir=args.srcdir,
                           fail_injector=FailInjector() if args.fail else None)
    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        httpd.server_close()


if __name__ == "__main__":
    sys.exit(main())

import codecs
from http.server import BaseHTTPRequestHandler
from socketserver import ThreadingMixIn
from json import dump
from tempfile import NamedTemporaryFile


class TreehubServer(BaseHTTPRequestHandler):
    def __init__(self, ostree_repo, *args):
        self._ostree_repo = ostree_repo
        BaseHTTPRequestHandler.__init__(self, *args)

    def _ostree_object(self):
        if self.path.startswith('/objects/'):
            return self.path[len('/objects/'):]
        else:
            return None

    def do_POST(self):
        obj = self._ostree_object()
        if self.path == '/token':
            self._respond({'access_token': "dummytoken123"})
        elif obj:
            code = self._ostree_repo.upload(obj)
            self.send_response_only(code)
            self.end_headers()
            if code == 500:
                self.wfile.write(b"Internal server error")
        elif self.path.startswith('/refs/heads'):
            self.send_response_only(200)
            self.end_headers()
            self.wfile.write(b"todo")
        else:
            print("Path was", self.path)
            self.send_response_only(400)
            self.end_headers()

    def do_HEAD(self):
        obj = self._ostree_object()
        if obj:
            res_code = self._ostree_repo.query(obj)
            print("HEAD %s -> %d" % (self.path, res_code))
            self.send_response_only(res_code)
            self.end_headers()
        else:
            self.send_response_only(400)
            self.end_headers()

    def _respond(self, json):
        self.send_response_only(200)
        self.send_header('Content-type', 'text/json')
        self.end_headers()
        writer = codecs.getwriter('UTF8')(self.wfile)
        dump(json, writer)
        writer.flush()


class TemporaryCredentials(object):
    TEMPLATE = u"""
    {
      "oauth2": {
        "client_id":"c169e6d6-d2de-4c56-ac8c-8b7671097e0c",
        "client_secret":"secret",
        "server":"http://localhost:%s"
      },
      "ostree":{
        "server":"http://localhost:%s"
      }
    }
    """

    def __init__(self, port):
        self._tmpfile = NamedTemporaryFile(mode='w+t')
        self._tmpfile.write(self.TEMPLATE % (port, port))
        self._tmpfile.flush()

    def path(self):
        return self._tmpfile.name

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self._tmpfile.__exit__(exc_type, exc_val, exc_tb)
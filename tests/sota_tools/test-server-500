#! /usr/bin/env python3

"""
Regression test for a bug caused by the logic that shuts down the request pool when
the server returns an error. This test causes 3 'query' requests to hang until
an upload fails.  Once the upload has failed and the server is trying to
shutdown, the queries are released. In the broken case this caused RequestPool
to try and process them, resulting in a hang.
"""
from mocktreehub import TreehubServer, TemporaryCredentials
from socketserver import ThreadingTCPServer
import subprocess
import threading

import sys

from time import sleep


class OstreeRepo(object):
    def __init__(self):
        self.query_count = 0
        self.upload_count = 0
        self.did_return_500 = threading.Event()
        self.queries_queued = 0

    def upload(self, name):
        print("Uploaded", name)
        self.upload_count += 1
        if self.upload_count % 2 == 0:
            self.did_return_500.set()
            return 500
        return 204

    def query(self, name):
        self.query_count += 1
        if name.endswith('.commit'):
            return 404
        elif name.endswith('.dirmeta') or name.endswith('.dirtree'):
            return 404
        elif self.query_count % 5 == 0:
            if self.queries_queued < 3:
                self.queries_queued += 1
                self.did_return_500.wait()
                sleep(2)
            return 404
        else:
            return 200


def main():
    ostree_repo = OstreeRepo()

    def handler(*args):
        TreehubServer(ostree_repo, *args)

    httpd = ThreadingTCPServer(('localhost', 0), handler)
    address, port = httpd.socket.getsockname()
    print("Serving at port", port)
    t = threading.Thread(target=httpd.serve_forever)
    t.setDaemon(True)
    t.start()

    target = sys.argv[1]

    with TemporaryCredentials(port) as creds:
        dut = subprocess.Popen(args=[target, '--credentials', creds.path(), '--ref', 'master',
                                     '--repo', 'bigger_repo'])
        try:
            exitcode = dut.wait(30)
            if not ostree_repo.did_return_500.is_set():
                print("Didn't hit failure case")
                sys.exit(1)
            if exitcode != 0:
                print("Incorrect exit code %d" % exitcode)
                sys.exit(1)
        except subprocess.TimeoutExpired:
            print("garage-push hung")
            sys.exit(1)


if __name__ == '__main__':
    main()

#! /usr/bin/env python3

from mocktreehub import TreehubServer, TemporaryCredentials
from socketserver import ThreadingTCPServer
import subprocess
import threading

import sys

from time import sleep


class OstreeRepo(object):
    def __init__(self):
        self.upload_count = 0

    def upload(self, name):
        print("Uploaded", name)
        self.upload_count += 1
        if self.upload_count >= 20:
            return 500
        return 204

    def query(self, name):
        return 404


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
            exitcode = dut.wait(120)
            if exitcode == 0:
                sys.exit(1)
            else: 
                sys.exit(0)
        except subprocess.TimeoutExpired:
            print("garage-push hung")
            sys.exit(1)


if __name__ == '__main__':
    main()

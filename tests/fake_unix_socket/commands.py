#!/usr/bin/python3

import argparse
import os
import socket
import time

parser = argparse.ArgumentParser(os.path.basename(__file__))
parser.add_argument('socket', metavar='socket', help='path to socket file')
args = parser.parse_args()

sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
sock.connect(args.socket)
sock.sendall(b'{"fields":[')
time.sleep(0.2)
sock.sendall(b'],"varia')
time.sleep(0.2)
sock.sendall(b'nt":"Shutdown"}')
sock.close()

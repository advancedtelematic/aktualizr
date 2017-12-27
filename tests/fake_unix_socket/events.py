#!/usr/bin/python3

import argparse
import os
import socket

parser = argparse.ArgumentParser(os.path.basename(__file__))
parser.add_argument('socket', metavar='socket', help='path to socket file')
args = parser.parse_args()

sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
sock.connect(args.socket)

f = open(args.socket + '.txt', 'w')
f.write(sock.recv(200).decode("utf-8"))
f.close()

sock.close()

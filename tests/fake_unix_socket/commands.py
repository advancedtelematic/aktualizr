#!/usr/bin/python3

import socket
import time
sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
sock.connect("/tmp/sota-commands.socket")
sock.sendall(b'{"fields":[')
time.sleep(0.2)
sock.sendall(b'],"varia')
time.sleep(0.2)
sock.sendall(b'nt":"Shutdown"}')
sock.close()

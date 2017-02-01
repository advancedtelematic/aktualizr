import socket 
import time
sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
sock.connect("/tmp/sota-commands.socket")
sock.sendall('{"fields":[')
time.sleep(0.2)
sock.sendall('],"varia')
time.sleep(0.2)
sock.sendall('nt":"GetUpdateRequests"}')
sock.close()

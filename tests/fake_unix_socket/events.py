import socket 
sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
sock.connect("/tmp/sota-events.socket")

f = open('/tmp/sota-events.socket.txt', 'w')
f.write(sock.recv(200))
f.close()

sock.close()

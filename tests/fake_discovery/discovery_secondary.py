#!/usr/bin/python3

import socket
import sys

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

server_address = ('localhost', int(sys.argv[1]))
print('starting up on {} port {}'.format(*server_address))
sock.bind(server_address)

print('\nwaiting to receive message')
data, address = sock.recvfrom(4096)

if data[:1] == b'\xa0':
    ecu1 = b'\xa1\x80\x30\x80\x0c\x08test_ecu\x0c\x10test_hardware_id\x02\x02\x23\x46\x00\x00\x00\x00'
    ecu2 = b'\xa1\x80\x30\x80\x0c\x09test_ecu2\x0c\x11test_hardware_id2\x02\x02\x23\x46\x00\x00\x00\x00'
    sent = sock.sendto(ecu1, address)
    sent = sock.sendto(ecu2, address)
else:
    print('Ignoring message %r' % data)

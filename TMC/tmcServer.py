#!/user/bin/env python

from socket import *
from time import ctime

HOST = ''
PORT = 21567
BUFSIZ = 1024
ADDR = (HOST, PORT)

udpSerSock = socket(AF_INET, SOCK_DGRAM)
udpSerSock.bind(ADDR)

connection, addr = udpSerSock.recvfrom(BUFSIZ)
print(connection)
sequence = 1

while True:
	data = str(sequence)
	udpSerSock.sendto(str.encode(data), addr)
	sequence = sequence + 1

udpSerSock.close()

#!/usr/bin/env python3

from socket import *

HOST = '45.76.50.110'
PORT = 21567
BUFSIZ = 1024
ADDR = (HOST, PORT)

udpCliSock = socket(AF_INET, SOCK_DGRAM)
data = "connection"
udpCliSock.sendto(str.encode(data), ADDR)

recv_count = 0

while True:
	print("client")
	data, addr = udpCliSock.recvfrom(BUFSIZ)
	recv_count = recv_count + 1
	print(data)
	sum_packet = int(data.decode('UTF-8'))
	loss_rate = 1 - recv_count/sum_packet
	file1 =  open("lossrate.txt", 'a')
	file1.write(str(recv_count)+'  '+str(sum_packet)+'  '+str(loss_rate)+'\n')
	file1.close()
udpCliSock.close()

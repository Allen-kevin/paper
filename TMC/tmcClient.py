#!/usr/bin/env python3

from socket import *

HOST = '119.29.183.89'
PORT = 21567
BUFSIZ = 1024
ADDR = (HOST, PORT)

udpCliSock = socket(AF_INET, SOCK_DGRAM)
data = "connection"
udpCliSock.sendto(str.encode(data), ADDR)

recv_count = 0
max_number = 0

while True:
    print("client")
    data, addr = udpCliSock.recvfrom(BUFSIZ)
    recv_count = recv_count + 1
    print(data)
    sum_packet = int(data.decode('UTF-8'))
    if sum_packet > max_number:
    	max_number = sum_packet
    loss_rate = 1 - recv_count/sum_packet
    file1 =  open("lossrate.txt", 'a')
    file1.write(str(recv_count)+'  '+str(sum_packet)+'  '+str(loss_rate)+'\n')
    file1.close()
udpCliSock.close()

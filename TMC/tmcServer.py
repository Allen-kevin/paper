#!/user/bin/env python

from socket import *
import time
import CircularQueue

HOST = ''
PORT = 21567
BUFSIZ = 1024
ADDR = (HOST, PORT)

udpSerSock = socket(AF_INET, SOCK_DGRAM)
udpSerSock.bind(ADDR)

connection, addr = udpSerSock.recvfrom(BUFSIZ)
print(connection)
sequence = 1
qStr = SqQueue(30)
qTime = SQQueue(30)

while True:
	data = str(sequence)
	t = time.time()
	if q.QueueFull()
		rate = qStr.SumStrQueue()/qTime.SumTimeQueue()
		print(rate/(1024*1024), end = '\n')
		if rate/(1024*1024) > 50:
			sleepTime = qStr.SumStrQueue()/50
			time.sleep(sleepTime)
		qTime.DeQueue()
		udpSerSock.sendto(str.encode(qStr.DeQueue()), addr)
		sequence = sequence + 1
		qStr.EnQueue(data)
		qTime.EnQueue(t)
	else:
		qStr.EnQueue(data)
		qTime.EnQueue(time)

udpSerSock.close()

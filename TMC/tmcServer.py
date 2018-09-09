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
qStr = CircularQueue.SqQueue(30)
qTime = CircularQueue.SqQueue(30)
minTime = time.time()

while True:
	t = time.time()

	if t - minTime > 60:
		break

	if qStr.QueueFull():
		print(qTime.SumTimeQueue())
		print(qStr.SumStrQueue())
		rate = qStr.SumStrQueue()/qTime.SumTimeQueue()
		print(rate/(1024*1024), end = '\n')
		if rate/(1024*1024) > 0.0001:
			sleepTime = qStr.SumStrQueue()/(0.0001*1024*1024) - qTime.SumTimeQueue()
			time.sleep(sleepTime)
		qTime.DeQueue()
		udpSerSock.sendto(str.encode(qStr.DeQueue()), addr)
		#sequence = sequence + 1
		qStr.EnQueue(str(sequence))
		qTime.EnQueue(t)
	else:
		#data = str(sequence)
		#print(sequence)
		qStr.EnQueue(str(sequence))
		qTime.EnQueue(t)

	sequence = sequence + 1

udpSerSock.close()

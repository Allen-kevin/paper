#!/usr/bin/env python

class SqQueue(object):
	def __init__(self, maxsize):
		self.queue = [None]*maxsize
		self.maxsize = maxsize
		self.front = 0
		self.rear = 0

	def QueueLength(self):
		return (self.rear - self.front + self.maxsize)% self.maxsize

	def QueueFull(self):
		return (self.rear + 1)%self.maxsize == self.front
	
	def QueueEmpty(self):
		return self.rear == self.front

	def EnQueue(self, data):
		if not self.QueueFull():
			self.queue[self.rear] = data
			self.rear = (self.rear + 1)%self.maxsize
		else:
			print("The queue is full!")
	
	def DeQueue(self):
		if not self.QueueEmpty():
			data = self.queue[self.front]
			self.queue[self.front] = None
			self.front = (self.front+1)%self.maxsize
			return data
		else:
			print("The queue is empty!")
	
	def ShowQueue(self):
		for i in range(self.maxsize):
			print(self.queue[i], end = ',')
		print('\n')

	def SumTimeQueue(self):
		return self.queue[rear] - self.queue[front]

	def SumStrQueue(sefl):
		return maxsize

if __name__ == "__main__":
	q = SqQueue(15)
	for i in range(10):
		q.EnQueue(i)
	q.ShowQueue()

	for i in range(5):
		q.DeQueue()
	q.ShowQueue()

	for i in range(8):
		q.EnQueue(i)
	q.ShowQueue()


#! /usr/bin/env python

import os
import matplotlib.pyplot as plt
import numpy as np

filepath = "/home/kevin/repositories/paper/TMC/oneBytes"

def drawing_x_time_y_lossRate():
	files = os.listdir(filepath)
	for filename in files:
		if not os.path.isdir(filename):	
			x = [] 
			y = []
			plt.figure()
			with open(filepath +"/"+filename, 'r') as f:
				lines = f.readlines()
				for line in lines:
					value = [float(s) for s in line.split()]
					x.append(value[0])
					y.append(value[3])

			plt.plot(x, y)
			plt.xlabel("time/s")
			plt.ylabel("loss rate")
			title = "rate = " + filename.split(".t")[0].split("_")[-1] + "MB/s"
			plt.title(title)
			#plt.legend()
			savename = filename.split(".t")[0] + ".png"
			plt.savefig(savename)
			plt.show()


def drawing_x_sendRate_y_lossRate():
	files = os.listdir(filepath)
	d = {}
	x = []
	y = []
	plt.figure()

	for filename in files:
		if not os.path.isdir(filename):	
			with open(filepath +"/"+filename, 'r') as f:
				lines = f.readlines()
				y_value = round(float(lines[-1].split(" ")[-1]), 4)
				x_value = filename.split(".t")[0].split("_")[-1]
				d[x_value] = y_value

	sorted(d.keys())
	for key in sorted(d):
		print(key)
		x.append(key)
		y.append(d[key])

	plt.plot(x, y)
	plt.xlabel("sending rate")
	plt.ylabel("loss rate")
	title = "packet size = 1bytes"
	plt.title(title)
	#plt.legend()
	savename = title.split(" ")[-1] + ".png"
	plt.savefig(savename)
	plt.show()

if __name__ == "__main__":
#	drawing_x_time_y_lossRate()
	drawing_x_sendRate_y_lossRate()

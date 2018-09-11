#! /usr/bin/env python3

import matplotlib.pyplot as plt
import numpy as np

filename = ".txt"
x = [] 
y = []

plt.figure()
with open(filename, r) as f:
    lines = f.readlines()
    for line in lines:
        value = [float(s) for s in line.split()]
        x.append(line[0])
        y.append(line[1])

plt.plot(x, y)
plt.show()

#!/usr/bin/env python

import os
import matplotlib
matplotlib.use ('agg')
import numpy as np
import matplotlib.pyplot as plt
import fileinput

def figure_time_packets():
    date = np.loadtxt('/home/shenyifan/data/probe.txt', 'r')
    plt.figure()
    plt.plot(data[:, 0], data[:, -1], 'o')
    plt.xlabel('time/s')
    plt.ylabel('walk time')
    plt.savefig('./walk_time.pdf', farmat='pdf')
    plt.close()

figure_time_packets()

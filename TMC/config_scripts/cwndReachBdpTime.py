#!/usr/bin/python

import os
import sys

MTU = 1500
delay = 0.15
alpha = 2
beta = 1

def CwndReachBdpTime(target):
    for line in open('/home/wanwenkai/data/probe.txt'):
        odom = line.split()
        if float(odom[6]) > float(target):
            return float(odom[0])
    return "not reach!"

def main():

    target1 = 10*1024*1024*1024*delay/(8*MTU)
    oneBdpTime = CwndReachBdpTime(target1)
    print oneBdpTime
    target2 = alpha*beta*10*1024*1024*1024*delay/(8*MTU)
    twoBdpTime = CwndReachBdpTime(target2)
    print twoBdpTime

if __name__ == '__main__':
    main()


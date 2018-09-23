#! /bin/bash
ifconfig eth4 192.168.125.208/24 up
ethtool -K eth4 tso off gso off gro off sg off 


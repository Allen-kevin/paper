#!/bin/bash
ifconfig eth4 192.168.125.208/24 up
#ethtool -K eth4 tso on gro on gso on sg on lro on
ethtool -K eth4 tso off gro off gso off sg off lro off
echo 4096 87830 1311232  > /proc/sys/net/ipv4/tcp_wmem
echo 4096 87830 1311232  > /proc/sys/net/ipv4/tcp_rmem
#insmod /home/wanwenkai/kernel/3.10.25.6/net/ipv4/tcp_probe.ko
insmod /home/wanwenkai/kernels/linux-4.10.13/net/ipv4/tcp_probe.ko
insmod /home/wanwenkai/kernels/linux-4.10.13/net/ipv4/tcp_bbr.ko
echo bbr > /proc/sys/net/ipv4/tcp_congestion_control
#echo 3 > /proc/sys/net/ipv4/tcp_sndbuf_factor
#echo 3 > /proc/sys/net/ipv4/tcp_queue_threshold
#echo 2 > /proc/sys/net/ipv4/tcp_congestion_threshold
#insmod /home/wanwenkai/kernel/3.10.25.5/net/ipv4/tcp_probe.ko
#arp -s 192.168.125.128 90e2ba0e36e4
#arp -s 192.168.125.209 90e2ba0e3344
service iptables stop
#echo bbr > /proc/sys/net/ipv4/tcp_congestion_control
#echo 0 > /proc/sys/net/ipv4/tcp_no_metrics_save 

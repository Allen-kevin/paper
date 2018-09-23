#! /bin/bash
#time,snd_nxt,snd_cwnd,ssthresh,snd_wnd
#cat /proc/net/tcpprobe  | awk '/192.168.123.212/{print $1"\t"$5"\t"$7"\t"$8"\t"$9}' > /home/wanwenkai/data/probe.txt &
#sysctl -p

cat /proc/net/tcpprobe > /home/wanwenkai/data/probe.txt &
pid_probe=$!

tcpdump -i eth4 tcp -s 100 -w /home/wanwenkai/data/dump.pcap &
#tcpdump -i eth3 tcp src 192.168.123.208 dst 192.168.123.212  > /home/wanwenkai/data/text.txt &
pid_dump=$!

sleep 1

#iperf -c 192.168.123.212 -P 1 -t 100
iperf -c 192.168.125.212 -i 1 -P 40 -t 100

tail -n 1000000 /var/log/messages > /home/wanwenkai/data/kernel.txt
sleep 1
kill $pid_probe
kill $pid_dump

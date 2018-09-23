#! /bin/bash
killall -9 iperf
killall -9 tcpdump
rm -f /home/shenyifan/data/probe.txt
rm -f /home/shenyifan/data/dump.pcap
#time,snd_nxt,snd_cwnd,ssthresh,snd_wnd
#cat /proc/net/tcpprobe  | awk '/192.168.125.128/{print $1"\t"$5"\t"$7"\t"$8"\t"$9}' > /home/shenyifan/data/probe.txt &
cat /proc/net/tcpprobe | grep 192.168.125.208 > /home/shenyifan/data/probe.txt &
#cat /proc/net/tcpprobe &
pid_probe=$!

#tcpdump -i eth10 tcp > /home/shenyifan/data/tcpdump.txt &
tcpdump -i eth6 -s 100 -w /home/shenyifan/data/dump.pcap tcp port 5001 &
#tcpdump -i eth2 -s 100 -w /home/shenyifan/data/dump.pcap tcp port 5001 &
#tcpdump -i eth10 tcp src 192.168.123.208 dst 192.168.123.212  > /home/shenyifan/data/text.txt &
pid_dump=$!
sleep 1


#echo 3 > /proc/sys/net/ipv4/tcp_sndbuf_factor
for prt in bbr
do
	#echo $prt > /proc/sys/net/ipv4/tcp_retransmission_algorithm
	echo $prt > /proc/sys/net/ipv4/tcp_congestion_control
	echo current retrans_algorithm is: $prt
    iperf -c 192.168.125.209 -i 1 -t 20
    #iperf -c 172.16.11.128 -i 1 -t 20
    sleep 5
done

kill $pid_probe
kill $pid_dump

#! /bin/bash
#time,snd_nxt,snd_cwnd,ssthresh,snd_wnd
#cat /proc/net/tcpprobe  | awk '/192.168.123.212/{print $1"\t"$5"\t"$7"\t"$8"\t"$9}' > /home/wanwenkai/data/probe.txt &
cat /proc/net/tcpprobe > /home/wanwenkai/data/probe.txt &
pid_probe=$!

tcpdump -i eth10 tcp -w /home/wanwenkai/data/dump.pcap &
#tcpdump -i eth10 tcp src 192.168.123.208 dst 192.168.123.212  > /home/wanwenkai/data/text.txt &
pid_dump=$!

sleep 1

for cong in $(seq 4)
do
	if [$cong=0]
	then
		echo cubic > /proc/sys/net/ipv4/tcp_congestion_control
		
	elif [$cong=1]
	then
		echo veno > /pro/sys/net/ipv4/tcp_congestion_control
	elif [$cong=2]
		echo vegas > /pro/sys/net/ipv4/tcp_congestion_control
	else 
		echo westwood > /pro/sys/net/ipv4/tcp_congestion_control
	fi

	for re in $(seq 4)
	do
		if [$re=0]
		then
			echo RH > /proc/sys/net/ipv4/tcp_retransmission_algorithm
		elif [$re=1]
		then
			echo PRR > /proc/sys/net/ipv4/tcp_retransmission_algorithm
		elif [$re=2]
		then
			echo SARR > /proc/sys/net/ipv4/tcp_retransmission_algorithm
		else
			echo PQRC > /proc/sys/net/ipv4/tcp_retransmission_algorithm
		fi

		for var in $(seq 5)
		do
			iperf -c 192.168.123.212 -i 1 -t 200
		done
	done
done

#tail -n 1000000 /var/log/messages > /home/shenyifan/data/kernel.txt
sleep 1
kill $pid_probe
kill $pid_dump

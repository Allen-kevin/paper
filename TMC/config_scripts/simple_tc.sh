tc qdisc del dev eth6 root
#tc qdisc add dev eth10 root
#tc qdisc add dev eth6 root handle 1:0 tbf rate 100mbit burst 10kb limit 10M
#tc qdisc add dev eth10 root loss 0.1%
#tc qdisc add dev eth10 parent 1:0 handle 10: netem loss 0.1% 
#tc qdisc add dev eth6 parent 1:0 handle 10: netem delay 50ms
# tc -s -d qdisc show dev eth10
#tc qdisc del dev eth10 root

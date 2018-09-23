#! /bin/bash
ovs-vsctl -- set Port eth2 qos=@newqos \
-- --id=@newqos create QoS type=linux-htb other-config:max-rate=1000000000 queues=0=@q0,1=@q1 \
-- --id=@q0 create Queue other-config:min-rate=100000000 other-config:max-rate=100000000 \
-- --id=@q1 create Queue other-config:min-rate=500000000

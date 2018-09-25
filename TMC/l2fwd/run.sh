#! /bin/bash
export RTE_SDK=/home/wanwenkai/dpdk-2.0.0
export RTE_TARGET=x86_64-native-linuxapp-gcc
rm -f ./build/l2fwd
make
./build/l2fwd -c 7 -n 4 -- -p 3 -T 2 2>err

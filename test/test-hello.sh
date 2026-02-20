#!/bin/bash

if (($EUID != 0)); then
    echo "You need to run this with root priviledge" >& 2
    exit 1
fi

WORKPLACE=$(dirname "$0")
cd $WORKPLACE

make clean
make
mount -o loop --mkdir images/tmp/ext4-a0 /tmp/ext4-a0
./test-hello

sleep 1 # add some delay to wait for printk buffer flushed

if dmesg | grep "bad apple" > /dev/null; then
    echo "OK"
else
    echo "Failed"
fi

umount images/tmp/ext4-a0


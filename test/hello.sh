#!/bin/bash

export TEVFS_WORKSPACE=$(dirname "$0")
cd $TEVFS_WORKSPACE

source ./img-var.sh

rm -f $TEVFS_IMAGEDIR

echo "$TEVFS_SIZE $TEVFS_NUM_INODES $TEVFS_IMAGEDIR $TEVFS_MOUNTPT"

truncate -s $TEVFS_SIZE $TEVFS_IMAGEDIR

if ! sudo -E ./img-mkfs.sh; then
    exit 1
fi

if ! sudo -E ./img-mount.sh; then
    exit 1
fi

make clean &> /dev/null
make &> /dev/null

echo "Running build/hello.x $TEVFS_MOUNTPT"
build/hello.x $TEVFS_MOUNTPT

sleep 1 # add some delay to wait for printk buffer flushed

if dmesg | grep "bad apple" > /dev/null; then
    echo "OK"
else
    echo "Failed"
fi

if ! sudo -E ./img-umount.sh; then
    exit 1
fi


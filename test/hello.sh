#!/bin/bash

export TEVFS_WORKSPACE=$(dirname "$0")
cd $TEVFS_WORKSPACE

source ./img-var.sh

rm -f $TEVFS_IMAGEPATH

echo "$TEVFS_SIZE $TEVFS_NUM_INODES $TEVFS_IMAGEPATH $TEVFS_MOUNTPT"

truncate -s $TEVFS_SIZE $TEVFS_IMAGEPATH

if ! sudo -E ./img-mkfs.sh; then
    exit 1
fi

if ! sudo -E ./img-mount.sh; then
    exit 1
fi

rm build/hello.x
make build/hello.x &> /dev/null

echo "Running build/hello.x $TEVFS_MOUNTPT"
build/hello.x $TEVFS_MOUNTPT

if dmesg | grep "bad apple" > /dev/null; then
    echo "OK"
else
    echo "Failed"
fi

if ! sudo -E ./img-umount.sh; then
    exit 1
fi


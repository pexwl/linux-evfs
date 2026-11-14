#!/bin/bash

cd $(dirname "$0")

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

mkdir "$TEVFS_MOUNTPT/hello"
echo "Running build/hello.x $TEVFS_MOUNTPT/hello"
build/hello.x "$TEVFS_MOUNTPT/hello"

if dmesg | grep "bad apple" > /dev/null; then
    echo "OK"
else
    echo "Failed"
fi

if ! sudo -E ./img-unmount.sh; then
    exit 1
fi


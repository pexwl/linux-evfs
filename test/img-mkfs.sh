#!/bin/bash
# this script should prepare the image and mount the file system

if (($EUID != 0)); then
    echo "You need ROOT priviledge"
    exit 1
fi

truncate -s $TEVFS_SIZE $TEVFS_IMAGEPATH
mkfs.ext4 $TEVFS_IMAGEPATH -b $TEVFS_BLOCK_SIZE -N $TEVFS_NUM_INODES

p3id=$(ps -o ppid= -p $PPID | tr -d ' ')
p4id=$(ps -o ppid= -p $p3id | tr -d ' ')
owner=$(ps -o user= -p $p4id | tr -d ' ')

chown "$owner":"$owner" $TEVFS_IMAGEPATH


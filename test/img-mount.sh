#!/bin/bash
# this script should prepare the image and mount the file system

if (($EUID != 0)); then
    echo "You need ROOT priviledge"
    exit 1
fi

mkdir -p $TEVFS_MOUNTPT

losetup /dev/loop0 $TEVFS_IMAGEPATH
if [[ $1 == "--writeback" ]]; then
	mount -t ext4 -o data=writeback /dev/loop0 $TEVFS_MOUNTPT
elif [[ $1 == "--ordered" ]]; then
	mount -t ext4 -o data=ordered /dev/loop0 $TEVFS_MOUNTPT
else
	mount -t ext4 -o data=journal /dev/loop0 $TEVFS_MOUNTPT
fi

chown code:code $TEVFS_MOUNTPT


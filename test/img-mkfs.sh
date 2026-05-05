#!/bin/bash
# this script should prepare the image and mount the file system

if (($EUID != 0)); then
    echo "You need ROOT priviledge"
    exit 1
fi

if [[ ! -v TEVFS_WORKSPACE ]]; then
    echo "workspace check fail"
    exit 1
fi

mkfs.ext4 $TEVFS_IMAGEDIR -b 1024 -N $TEVFS_NUM_INODES

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

mkdir -p $TEVFS_MOUNTPT
mount -o loop,data=journal $TEVFS_IMAGEDIR $TEVFS_MOUNTPT
chown code:code $TEVFS_MOUNTPT

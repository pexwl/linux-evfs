#!/bin/bash
# this script should umount the file system and remove it

if (($EUID != 0)); then
    echo "You need ROOT priviledge"
    exit 1
fi

umount $TEVFS_MOUNTPT
losetup -d /dev/loop0


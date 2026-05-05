#!/bin/bash

export TEVFS_WORKSPACE=$(dirname "$0")
cd $TEVFS_WORKSPACE

source ./img-var.sh

echo "$TEVFS_SIZE $TEVFS_NUM_INODES $TEVFS_IMAGEDIR $TEVFS_MOUNTPT"
rm -f $TEVFS_IMAGEDIR

truncate -s $TEVFS_SIZE $TEVFS_IMAGEDIR

if ! sudo -E ./img-mkfs.sh; then
    exit 1
fi

block_num=2048

if ! sudo -E ./img-mount.sh; then
    exit 1
fi

make clean &> /dev/null
make &> /dev/null

echo "running ./balloc.x $block_num $TEVFS_MOUNTPT"
./balloc.x $block_num $TEVFS_MOUNTPT

if ! sudo -E ./img-umount.sh; then
    exit 1
fi

sudo sync # sync manually

# skip fsck because we have a block not pointed to by any inode
echo ""
echo "------ new debugfs test ------"
res=$(sudo -E debugfs -n -R "testb $block_num" $TEVFS_IMAGEDIR)
echo "$res"

echo ""
echo "------- new fsck test ---------"
sudo -E fsck.ext4 $TEVFS_IMAGEDIR

if echo $res | grep "marked in use" > /dev/null; then
    echo OK
else
    echo Failed
fi

#!/bin/bash

export TEVFS_WORKSPACE=$(dirname "$0")
cd $TEVFS_WORKSPACE

source ./img-var.sh

echo "$TEVFS_SIZE $TEVFS_NUM_INODES $TEVFS_IMAGEPATH $TEVFS_MOUNTPT"
rm -f $TEVFS_IMAGEPATH

truncate -s $TEVFS_SIZE $TEVFS_IMAGEPATH

if ! sudo -E ./img-mkfs.sh; then
    exit 1
fi

if ! sudo -E ./img-mount.sh; then
    exit 1
fi

rm build/balloc.x
make build/balloc.x &> /dev/null

block_num=2048
echo "running build/balloc.x $block_num $TEVFS_MOUNTPT"
build/balloc.x $block_num $TEVFS_MOUNTPT
echo "exit code: $?"

if [[ $1 == "--no-unmount" ]]; then
    exit 0
fi

if ! sudo -E ./img-umount.sh; then
    exit 1
fi

# skip fsck because we have a block not pointed to by any inode
echo ""
echo "------ new debugfs test ------"
res=$(sudo -E debugfs -R "testb $block_num" $TEVFS_IMAGEPATH)
echo "$res"

echo ""
echo "------- new fsck test ---------"
sudo -E fsck.ext4 $TEVFS_IMAGEPATH

if echo $res | grep "marked in use" > /dev/null; then
    echo OK
else
    echo Failed
fi

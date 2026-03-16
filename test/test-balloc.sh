#!/bin/bash

export TEVFS_WORKSPACE=$(dirname "$0")
cd $TEVFS_WORKSPACE

source ./test-img-var.sh

echo "$TEVFS_SIZE $TEVFS_NUM_INODES $TEVFS_IMAGEDIR $TEVFS_MOUNTPT"

truncate -s $TEVFS_SIZE $TEVFS_IMAGEDIR

if ! sudo -E ./test-img-mount.sh; then
    exit 1
fi

make clean
make
echo "running ./test-balloc $TEVFS_MOUNTPT"
./test-balloc $TEVFS_MOUNTPT

if ! sudo -E ./test-img-umount.sh; then
    exit 1
fi

sudo sync # sync manually

# skip fsck because we have a block not pointed to by any inode
res=$(sudo -E debugfs -n -R "testb 2048" $TEVFS_IMAGEDIR)
echo $res

if echo $res | grep "marked in use" > /dev/null; then
    echo OK
else
    echo Failed
fi
rm $TEVFS_IMAGEDIR

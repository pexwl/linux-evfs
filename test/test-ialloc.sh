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

time0=$(date -d "now" "+%s")

# let's verify that the node is not valid before we run our program
sudo -E debugfs -n -R "stat <111>" $TEVFS_IMAGEDIR | head -n 32

echo "running ./test-ialloc $TEVFS_MOUNTPT"
./test-ialloc $TEVFS_MOUNTPT

if ! sudo -E ./test-img-umount.sh; then
    exit 1
fi

sudo sync # manually sync

# skip fsck because we have no diretory entries referring to the inode
res=$(sudo -E debugfs -n -R "stat <111>" $TEVFS_IMAGEDIR | grep atime | awk '{print $4, $5, $6, $7, $8}')
echo "atime: $res"
time2=$(date -d "now" "+%s")
time1=$(date -d "$res" "+%s")

if [ $time1 -ge $time0 ] && [ $time2 -ge $time1 ] ; then
    echo OK
else
    echo Failed
fi

rm $TEVFS_IMAGEDIR

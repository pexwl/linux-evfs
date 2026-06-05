#!/bin/bash

export TEVFS_WORKSPACE=$(dirname "$0")
cd $TEVFS_WORKSPACE

source img-var.sh

echo "$TEVFS_SIZE $TEVFS_NUM_INODES $TEVFS_IMAGEPATH $TEVFS_MOUNTPT"
rm -f $TEVFS_IMAGEPATH

truncate -s $TEVFS_SIZE $TEVFS_IMAGEPATH

if ! sudo -E ./img-mkfs.sh; then
    exit 1
fi

if ! sudo -E ./img-mount.sh; then
    exit 1
fi

dname="parent"
fname="child"
dir=$TEVFS_MOUNTPT/"$dname"
file=$dir/"$fname"

mkdir $dir
touch $file

sudo sync

res=$(stat "$dir")
pino_num=$(echo -e "$res" | head -n3 | tail -n1 | awk '{print $4}')
res=$(stat "$file")
cino_num=$(echo -e "$res" | head -n3 | tail -n1 | awk '{print $4}')

rm build/dentry_read.x
make build/dentry_read.x &> /dev/null

echo -e "\nrunning build/dentry_read.x $pino_num 2 $TEVFS_MOUNTPT"
res=$(build/dentry_read.x $pino_num 2 $TEVFS_MOUNTPT)
echo -e "$res"
echo "exit code: $?"

if ! sudo -E ./img-umount.sh; then
    exit 1
fi

if echo $res | grep "Inode: $cino_num" > /dev/null; then
    echo "OK"
else
    echo "Failed"
fi


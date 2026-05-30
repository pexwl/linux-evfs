#!/bin/bash

export TEVFS_WORKSPACE=$(dirname "$0")
cd $TEVFS_WORKSPACE

# add a dentry
echo -e "=== running dentry_add.sh ==="
./dentry_add.sh --no-unmount
echo -e "\n=== dentry_add.sh exit $? ==="

source img-var.sh
cino_num=59

dname="parent"
fname="child"
dir=$TEVFS_MOUNTPT/"$dname"
file=$dir/"$fname"

sudo sync

res=$(stat "$dir")
pino_num=$(echo -e "$res" | head -n3 | tail -n1 | awk '{print $4}')
echo -e "\nstat $dir\n$res"

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


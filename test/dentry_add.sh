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

rm build/ialloc.x
rm build/dentry_add.x
make build/ialloc.x &> /dev/null
make build/dentry_add.x &> /dev/null

cino_num=59

dname="parent"
dir=$TEVFS_MOUNTPT/"$dname"
mkdir $dir
res=$(stat "$dir")
pino_num=$(echo -e "$res" | head -n3 | tail -n1 | awk '{print $4}')

fname="child"
file=$dir/"$fname"

echo "running build/ialloc.x $cino_num $TEVFS_MOUNTPT"
build/ialloc.x $cino_num $TEVFS_MOUNTPT
echo "exit code: $?"

echo "running build/dentry_add.x $fname $pino_num $cino_num $TEVFS_MOUNTPT"
build/dentry_add.x $fname $pino_num $cino_num $TEVFS_MOUNTPT
echo "exit code: $?"

if [[ "$1" == "--no-unmount" ]]; then
    exit 0
fi

if ! sudo -E ./img-umount.sh; then
    exit 1
fi

echo ""
echo "------ new debugfs test ------"
res=$(sudo -E debugfs -R "testi /$dname/$fname" $TEVFS_IMAGEPATH)
echo "$res"

echo ""
echo "------- new fsck test ---------"
sudo -E fsck.ext4 $TEVFS_IMAGEPATH

if echo $res | grep "$cino_num is marked in use" > /dev/null; then
    echo OK
else
    echo Failed
fi

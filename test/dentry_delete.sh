#!/bin/bash

export TEVFS_WORKSPACE=$(dirname "$0")
cd $TEVFS_WORKSPACE

source img-var.sh

echo "$TEVFS_SIZE $TEVFS_NUM_INODES $TEVFS_IMAGEPATH $TEVFS_MOUNTPT"

truncate -s $TEVFS_SIZE $TEVFS_IMAGEPATH

if ! sudo -E ./img-mkfs.sh; then
    exit 1
fi

if ! sudo -E ./img-mount.sh; then
    exit 1
fi

# add a dentry
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
echo -e "\nstat $dir\n$res"

rm build/dentry_delete.x
make build/dentry_delete.x &> /dev/null

echo -e "\nrunning build/dentry_delete.x $pino_num $fname $TEVFS_MOUNTPT"
build/dentry_delete.x $pino_num $fname $TEVFS_MOUNTPT
echo "exit code: $?"

if ! sudo -E ./img-umount.sh; then
    exit 1
fi

echo ""
echo "------ new debugfs test ------"
res=$(sudo -E debugfs -R "testi <$cino_num>" $TEVFS_IMAGEPATH)
echo "$res"

echo ""
echo "------- new fsck test ---------"
sudo -E fsck.ext4 $TEVFS_IMAGEPATH

if echo $res | grep "not in use" > /dev/null; then
    echo "OK"
else
    echo "Failed"
fi


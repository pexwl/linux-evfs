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
touch "$file.tmp"
touch "$file"

sudo sync

res=$(stat "$dir")
pino_num=$(echo -e "$res" | head -n3 | tail -n1 | awk '{print $4}')
res=$(stat "$file")
cino_num=$(echo -e "$res" | head -n3 | tail -n1 | awk '{print $4}')

rm build/dentry_update.x
make build/dentry_update.x &> /dev/null

echo -e "\n------ initial: debugfs test ------"
res=$(sudo -E debugfs -R "ls -p <$pino_num>" $TEVFS_IMAGEPATH)
echo "$res"

echo -e "\nrunning build/dentry_update.x $pino_num 2 $cino_num $TEVFS_MOUNTPT"
res=$(build/dentry_update.x $pino_num 2 $cino_num $TEVFS_MOUNTPT)
echo -e "$res"
echo "exit code: $?"

if ! sudo -E ./img-umount.sh; then
    exit 1
fi

echo -e "\n------ updated: debugfs test ------"
res=$(sudo -E debugfs -R "ls -p <$pino_num>" $TEVFS_IMAGEPATH)
echo "$res"

echo -e "\n------- new fsck test ---------"
res2=$(sudo -E fsck.ext4 -f -n $TEVFS_IMAGEPATH)
echo -e "$res2"

c=$(echo -e "$res" | grep $cino_num | wc -l)
if [[ $c == 2 ]] ; then
    echo "OK"
else
    echo "Failed"
fi


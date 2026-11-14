#!/bin/bash

cd $(dirname "$0")

source ./img-var.sh

rm -f $TEVFS_IMAGEPATH

echo "$TEVFS_SIZE $TEVFS_NUM_INODES $TEVFS_IMAGEPATH $TEVFS_MOUNTPT"


if ! sudo -E ./img-mkfs.sh; then
    exit 1
fi

if ! sudo -E ./img-mount.sh; then
    exit 1
fi

make build/iread.x &> /dev/null

# create a file
file="$TEVFS_MOUNTPT/abc"
touch $file

sudo sync

res=$(stat "$file")
ino_num=$(echo -e "$res" | head -n3 | tail -n1 | awk '{print $4}')
echo -e "\n>>> build/iread.x $ino_num $file"
build/iread.x $ino_num $file
echo -e "<<<"
echo -e "\n>>> stat $ffile"
echo -e "$res"
echo -e "<<<"

if ! sudo -E ./img-unmount.sh; then
    exit 1
fi

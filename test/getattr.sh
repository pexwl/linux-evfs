#!/bin/bash
export TEVFS_WORKSPACE=$(dirname "$0")
cd $TEVFS_WORKSPACE

source ./img-var.sh

rm -f $TEVFS_IMAGEDIR

echo "$TEVFS_SIZE $TEVFS_NUM_INODES $TEVFS_IMAGEDIR $TEVFS_MOUNTPT"

truncate -s $TEVFS_SIZE $TEVFS_IMAGEDIR

if ! sudo -E ./img-mkfs.sh; then
    exit 1
fi

if ! sudo -E ./img-mount.sh; then
    exit 1
fi

make clean
make

# create a file
file="$TEVFS_MOUNTPT/abc"
touch $file
echo -e "\n>>> ./getattr.x $file"
./getattr.x $file
echo -e "<<<"

echo -e "\n>>> stat $file"
stat $file
echo -e "<<<"

rm $file
sudo sync
# create a directory
mkdir -p $file
echo -e "\n>>> ./getattr.x $file"
./getattr.x $file
echo -e "<<<"

echo -e "\n>>> stat $file"
stat $file
echo -e "<<<"

if ! sudo -E ./img-umount.sh; then
    exit 1
fi

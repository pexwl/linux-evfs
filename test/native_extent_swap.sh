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

if ! sudo -E ./img-mount.sh --no-data-journaling; then
    exit 1
fi

fname1="a"
fname2="b"
file1=$TEVFS_MOUNTPT/"$fname1"
file2=$TEVFS_MOUNTPT/"$fname2"

tr '\0' 'A' < /dev/zero | dd of=$file1 bs=1024 count=4 conv=fdatasync 2>/dev/null
tr '\0' 'B' < /dev/zero | dd of=$file2 bs=1024 count=4 conv=fdatasync 2>/dev/null

rm build/native_extent_swap.x
make build/native_extent_swap.x &> /dev/null

echo -e "\n=== before :: file 1 ==="
hexdump -C $file1

echo -e "\n=== before :: file 2 ==="
hexdump -C $file2

sudo sync
echo 3 | sudo tee /proc/sys/vm/drop_caches > /dev/null

echo -e "\nrunning build/extent_swap.x 0 1 $file1 $file2"
build/native_extent_swap.x 0 1 $file1 $file2
echo "exit code: $?"

sudo sync

echo -e "\n=== after :: file 1 ==="
hexdump -C $file1

echo -e "\n=== after :: file 2 ==="
res=$(hexdump -C $file2)
echo -e "$res"

echo ""

if ! sudo -E ./img-umount.sh; then
    exit 1
fi

if echo $res | grep "0400" > /dev/null; then
    echo "OK"
else
    echo "Failed"
fi


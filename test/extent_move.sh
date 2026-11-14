#!/bin/bash

cd $(dirname "$0")

source img-var.sh

echo "$TEVFS_SIZE $TEVFS_NUM_INODES $TEVFS_IMAGEPATH $TEVFS_MOUNTPT"
rm -f $TEVFS_IMAGEPATH

if ! sudo -E ./img-mkfs.sh; then
	exit 1
fi

if ! sudo -E ./img-mount.sh; then
	exit 1
fi

fname1="a"
file1=$TEVFS_MOUNTPT/"$fname1"
	
rm build/balloc.x build/bfree.x build/iver.x build/extent_move.c.x
make build/balloc.x build/bfree.x build/iver.x build/extent_move.c.x &> /dev/null

tr '\0' 'A' < /dev/zero | dd of=$file1 bs=1024 count=2 conv=fdatasync 2>/dev/null	
tr '\0' 'A' < /dev/zero | dd of=$file1 bs=1024 seek=3 count=2 conv=fdatasync 2>/dev/null
sudo sync

ino_num=$(stat $file1 | head -n3 | tail -n1 | awk '{print $4}')

# tr '\0' 'A' < /dev/zero | dd of=$file1 bs=1024 seek=6 count=1 conv=fdatasync 2>/dev/null
# sudo sync

echo -e "\n=== before :: file 1 ==="
hexdump -C $file1

echo ""
res=$(sudo debugfs -n -R "stat <$ino_num>" $TEVFS_IMAGEPATH)
echo -e "$res"

e_len=6
e_start=2001
e_end=$(( $e_start + $e_len - 1 ))
echo -e "\nrunning build/balloc.x $e_start-$e_end $TEVFS_MOUNTPT"
for (( i=2001; i < $(($e_start + $e_len)); i++ )); do
	build/balloc.x $i $TEVFS_MOUNTPT
done

sudo sync
echo 3 | sudo tee /proc/sys/vm/drop_caches > /dev/null

echo -e "\nrunning build/iver.x $file1"
iver=$(build/iver.x $file1)
echo -e "$iver"

echo -e "\nrunning build/extent_move.c.x $iver $ino_num 1 $e_start $e_len $TEVFS_MOUNTPT"
res=$(build/extent_move.c.x $iver $ino_num 1 $e_start $e_len $TEVFS_MOUNTPT)
echo -e "$res"

echo -e "\nexit code: $?"
sudo sync

echo -e "\n=== after :: file 1 ==="
hexdump -C $file1

echo ""
sudo debugfs -n -R "stat <$ino_num>" $TEVFS_IMAGEPATH | cat

echo ""
if ! sudo -E ./img-unmount.sh; then
	exit 1
fi

echo ""
echo "------- new fsck test ---------"
sudo fsck.ext4 -f -n $TEVFS_IMAGEPATH

nums=$(echo -e "$res" | tail +2 | awk '{print $3}')
sum=0
for num in $nums; do
	sum=$(($sum + $num))
done

if [ "$sum" -eq "$e_len" ]; then
	echo "OK"
else
	echo "Failed"
fi


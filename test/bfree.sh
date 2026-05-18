#!/bin/bash
export TEVFS_WORKSPACE=$(dirname "$0")
cd $TEVFS_WORKSPACE

# allocate the inode first without unmount
echo -e "\n=== running balloc.sh ==="
./balloc.sh --no-unmount
echo -e "\n=== balloc.sh exit $? ==="

# exit 0
read -p "Press enter to continue"
source ./img-var.sh

block_num=2048
echo -e "running build/bfree.x $block_num $TEVFS_MOUNTPT\n"
build/bfree.x $block_num $TEVFS_MOUNTPT
echo "exit code: $?"

if ! sudo -E ./img-umount.sh; then
    exit 1
fi

sudo sync # manually sync

echo -e "\n------ new debugfs check ------"
res=$(sudo -E debugfs -R "testb $block_num" $TEVFS_IMAGEDIR)
echo -e "$res"
echo -e "\n----- new filesystem check ----"
# observe what e2fsck do to the disk after inputing no to it
sudo -E e2fsck -f -n $TEVFS_IMAGEDIR

if echo $res | grep "not in use" > /dev/null; then
    echo -e OK
else
    echo -e Failed
fi

# rm $TEVFS_IMAGEDIR

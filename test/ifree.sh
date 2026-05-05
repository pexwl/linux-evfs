#!/bin/bash
export TEVFS_WORKSPACE=$(dirname "$0")
cd $TEVFS_WORKSPACE

# allocate the inode first
echo -e "\n=== running ialloc.sh ==="
./ialloc.sh
echo -e "\n=== ialloc.sh exit $? ==="

exit 0
read -p "Press enter to continue"

source ./img-var.sh

inode_num=55

if ! sudo -E ./img-mount.sh; then
    exit 1
fi

# TODO: there is no way to test this function until
# TODO: writing another function in ext4 ioctl
# TODO: that starts a journal and forcefully sets
# TODO: link count of an inode to 0

make clean &> /dev/null
make &> /dev/null

time0=$(date -d "now" "+%s")

echo -e "running ./ifree.x $inode_num $TEVFS_MOUNTPT\n"
./ifree.x $inode_num $TEVFS_MOUNTPT
echo $?

if ! sudo -E ./img-umount.sh; then
    exit 1
fi

sudo sync # manually sync

echo -e "\n------ new debugfs check ------"
res=$(sudo -E debugfs -R "testi <$inode_num>" $TEVFS_IMAGEDIR)
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

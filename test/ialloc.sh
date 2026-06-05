#!/bin/bash
export TEVFS_WORKSPACE=$(dirname "$0")
cd $TEVFS_WORKSPACE

source ./img-var.sh

echo -e "$TEVFS_SIZE $TEVFS_NUM_INODES $TEVFS_IMAGEPATH $TEVFS_MOUNTPT"
rm -f $TEVFS_IMAGEPATH
truncate -s $TEVFS_SIZE $TEVFS_IMAGEPATH

sudo sync

if ! sudo -E ./img-mkfs.sh; then
    exit 1
fi

inode_num=55

# let's verify that the node is not valid before we run our program
if [[ $1 != "--no-unmount" ]]; then
    echo -e "------ init debugfs check ------"
    sudo -E debugfs -n -R "stat <$inode_num>" $TEVFS_IMAGEPATH | head -n 32
    echo -e "--------- init fsck ------------"
    sudo -E fsck.ext4 $TEVFS_IMAGEPATH
fi

if ! sudo -E ./img-mount.sh; then
    exit 1
fi

rm build/ialloc.x
make build/ialloc.x &> /dev/null

time0=$(date -d "now" "+%s")

echo -e "running build/ialloc.x $inode_num $TEVFS_MOUNTPT"
build/ialloc.x $inode_num $TEVFS_MOUNTPT
ret=$?
echo "exit code: $ret"

if [[ $1 == "--no-unmount" ]]; then
    exit $ret
fi

echo ""

if ! sudo -E ./img-umount.sh; then
    exit 1
fi

echo -e "\n------ new debugfs check ------"
dfs_res=$(sudo -E debugfs -R "stat <$inode_num>" $TEVFS_IMAGEPATH)
res=$(echo -e "$dfs_res" | grep atime | awk '{print $4, $5, $6, $7, $8}')
echo -e "$dfs_res"
echo -e "\n----- new filesystem check ----"
# observe what e2fsck do to the disk after inputing no to it
sudo -E e2fsck -f -n $TEVFS_IMAGEPATH

echo -e "\n-------- self check -----------"
echo -e "atime: $res"
time2=$(date -d "now" "+%s")
time1=$(date -d "$res" "+%s")

if [ $time1 -ge $time0 ] && [ $time2 -ge $time1 ] ; then
    echo -e OK
else
    echo -e Failed
fi


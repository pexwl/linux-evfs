#!/bin/bash
export TEVFS_WORKSPACE=$(dirname "$0")
cd $TEVFS_WORKSPACE

# allocate the inode first without unmount
echo -e "=== running ialloc.sh ==="
./ialloc.sh --no-unmount
ret=$?
if [[ $ret != 0 ]]; then
     source ./img-var.sh
     sudo -E ./img-umount.sh
fi
echo -e "\n=== ialloc.sh exit $ret ==="

if [[ $ret != 0 ]]; then
    exit 1
fi

source ./img-var.sh
inode_num=55

rm build/ifree.x
make build/ifree.x &> /dev/null

sudo sync
echo -e "\nrunning build/ifree.x $inode_num $TEVFS_MOUNTPT"
build/ifree.x $inode_num $TEVFS_MOUNTPT
echo "exit code: $?"

if ! sudo -E ./img-umount.sh; then
    exit 1
fi

echo -e "\n------ new debugfs check ------"
res=$(sudo -E debugfs -R "testi <$inode_num>" $TEVFS_IMAGEPATH)
echo -e "$res"
echo -e "\n----- new filesystem check ----"
# observe what e2fsck do to the disk after inputing no to it
sudo -E e2fsck -f -n $TEVFS_IMAGEPATH

if echo $res | grep "not in use" > /dev/null; then
    echo -e OK
else
    echo -e Failed
fi

# rm $TEVFS_IMAGEPATH

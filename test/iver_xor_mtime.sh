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
echo -e "\ntesting on a file $file"
touch $file
prev_mtime=""
prev_ver=""
res=1
for ((i=0; i<256; i++)); do
	rand=$RANDOM
	op=$(expr $rand % 4)
	if [[ $op == 0 ]]; then
		echo $rand >> $file
	elif [[ $op == 1 ]]; then
		truncate -s $RANDOM $file
	elif [[ $op == 2 ]]; then
		mv $file "$TEVFS_MOUNTPT/$rand"
		file="$TEVFS_MOUNTPT/$rand"
	fi

	sudo sync
	

	curr_mtime=$(ls --full-time $file | tail -n1 | awk '{print $7,$8}' )
	curr_ver=$(./iver.x $file)

#	hashed_curr_mtime=$(echo $curr_mtime | sha256sum | awk '{print $1}')
#	hashed_prev_mtime=$(echo $curr_mtime | sha256sum | awk '{print $1}')
	
	[[ "$curr_mtime" == "$prev_mtime" ]] && time_match=1 || time_match=0
	[[ "$curr_ver" == "$prev_ver" ]] && ver_match=1 || ver_match=0
	if [[ $time_match != $ver_match ]]; then
		res=0
		break
	fi

	prev_mtime="$curr_mtime"
	prev_ver="$curr_ver"
done

echo ""

if [[ $res == 1 ]]; then
	echo "ok (i=$i)"
else
	echo "fail at iteration i=$i op=$op"
	echo "(time_match) $time_match, (ver_match) $ver_match"
	echo "prev: (ver) $prev_ver, (mtime) $prev_mtime"
	echo "curr: (ver) $curr_ver, (mtime) $curr_mtime"
fi

if ! sudo -E ./img-umount.sh; then
    exit 1
fi

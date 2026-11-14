#!/bin/bash
cd $(dirname "$0")

source ./img-var.sh

rm -f $TEVFS_IMAGEPATH

echo "$TEVFS_SIZE $TEVFS_NUM_INODES $TEVFS_IMAGEPATH $TEVFS_MOUNTPT"

truncate -s $TEVFS_SIZE $TEVFS_IMAGEPATH

if ! sudo -E ./img-mkfs.sh; then
    exit 1
fi

if ! sudo -E ./img-mount.sh; then
    exit 1
fi

make build/iver.x &> /dev/null

# create a file
file="$TEVFS_MOUNTPT/abc"
echo -e "\ntesting on a file $file"
touch $file
prev_mtime=""
prev_ver=""
res=1
prev_trc_sz=0
for ((i=0; i<256; i++)); do
	rand=$RANDOM
	op=$(expr $rand % 4)
	if [[ $op == 0 ]]; then
		# echo "echo $rand >> $file"
		echo $rand >> $file
	elif [[ $op == 1 ]]; then
		# make sure truncated size is different
		# to force ext4 metadata update
		curr_trc_sz=$RANDOM
		while [[ $curr_trc_sz == $prev_trc_sz ]]; do
			curr_trc_sz=$RANDOM
		done
		prev_trc_sz=$curr_trc_sz
		# echo "truncate -s $curr_trc_sz $file"
		truncate -s $curr_trc_sz $file
	elif [[ $op == 2 ]]; then
		# echo "mv $file $TEVFS_MOUNTPT/$rand"
		mv $file "$TEVFS_MOUNTPT/$rand"
		file="$TEVFS_MOUNTPT/$rand"
	fi

	sudo sync
	
	curr_mtime=$(ls --full-time $file | tail -n1 | awk '{print $7,$8}' )
	curr_ver=$(build/iver.x $file)
	
	[[ "$curr_mtime" != "$prev_mtime" ]] && time_changed=1 || time_changed=0
	[[ "$curr_ver" != "$prev_ver" ]] && ver_changed=1 || ver_changed=0
	if [[ $time_changed != $ver_changed ]]; then
		res=0
		break
	fi

	prev_mtime="$curr_mtime"
	prev_ver="$curr_ver"
done

echo ""

if [[ $res == 1 ]]; then
	echo "ok (i=$i)"
	echo "mtime changed <=> i_version changed"
else
	echo "fail at iteration i=$i op=$op"
	echo "(time_changed) $time_changed, (ver_changed) $ver_changed"
	echo "prev: (ver) $prev_ver, (mtime) $prev_mtime"
	echo "curr: (ver) $curr_ver, (mtime) $curr_mtime"
fi

if ! sudo -E ./img-unmount.sh; then
    exit 1
fi

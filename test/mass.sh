#!/bin/bash

for ((i=0; i<1024; i++)); do
	sudo sync
	sudo udevadm settle
	sleep 0.05
	
	res=$(./"$1" 2>&1)
	lres=$(echo -e "$res" | tail -n1)

	if [[ $res == "Failed" ]]; then
		echo -e "Fail at attempt $i/1024\n"
		echo -e "$res"
		break
	fi

	echo -ne "'$1' mass test ($i/1024)\r"
done
echo -e "'$1' mass test (1024/1024)\n"


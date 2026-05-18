cd $(dirname -- $0)/linux-6.8/

vim -p fs/ext4/evfs.c include/uapi/linux/evfs.h fs/ext4/evfs.h

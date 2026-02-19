#include "ext4.h"

#ifndef _EXT4_IOCTL_EVFS
#define _EXT4_IOCTL_EVFS
#define EXT4_IOC_EVFS_HELLO _IO('f', 99)
#define EXT4_IOC_EVFS_BLK_ALLOC _IOW('f', 100, int)
#endif

enum ERREVFS {
    ERRBLKSET = 134, // block already allocate
    ERRNOGRPD = 135, // no valid group descriptor; either group index out of range or rcu failed
};

long evfs_hello(void);
long evfs_journal_block_alloc(struct inode * ino, struct super_block * sb, ext4_fsblk_t bindex);
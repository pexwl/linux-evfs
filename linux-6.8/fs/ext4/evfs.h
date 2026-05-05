#include <linux/evfs.h>
#include "ext4.h"

#ifndef _EXT4_IOCTL_EVFS

#define _EXT4_IOCTL_EVFS

long ext4_ioctl_evfs(unsigned int cmd, struct inode * ino, struct super_block * sb, unsigned long arg);

#endif


#include <linux/fs.h>

#ifndef _EXT4_IOCTL_EVFS
#define _EXT4_IOCTL_EVFS

long ext4_evfs_inode_alloc_gbl(struct super_block *);
long ext4_ioctl_evfs(unsigned int, struct inode *, struct super_block *, unsigned long);

#endif


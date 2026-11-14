#include <linux/fs.h>

#ifndef _F2FS_IOCTL_EVFS
#define _F2FS_IOCTL_EVFS
#define F2FS_EVFS_SECS_MARGIN 4

long f2fs_ioctl_evfs(unsigned int cmd, struct file * filp, unsigned long arg);

#endif

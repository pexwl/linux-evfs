#include "ext4.h"

#ifndef EXT4_IOC_EVFS

#define EXT4_IOC_EVFS

struct evfs_stat
{
	unsigned long est_dev;
	unsigned long est_ino;
	unsigned int est_mode;
	unsigned int est_nlink;
	unsigned int est_uid;
	unsigned int est_gid;
	unsigned long est_rdev;
	unsigned long est_version;
	long est_size;
	int est_blksize;
	int __pad1;
	long est_blocks;
	long est_atime;
	unsigned long est_atime_nsec;
	long est_mtime;
	unsigned long est_mtime_nsec;
	long est_ctime;
	unsigned long est_ctime_nsec;
	unsigned int __unused4;
	unsigned int __unused5;
};

#define EXT4_IOC_EVFS_HELLO _IO('f', 99)
#define EXT4_IOC_EVFS_IVER _IOR('f', 100, unsigned long long)
#define EXT4_IOC_EVFS_GETATTR _IOR('f', 101, struct evfs_stat)
#define EXT4_IOC_EVFS_BLK_ALLOC _IO('f', 102)
#define EXT4_IOC_EVFS_INO_ALLOC _IO('f', 103)
#define EXT4_IOC_EVFS_INO_FREE _IO('f', 104)

#endif


#include <linux/types.h>

#ifndef _GENERIC_EVFS
#define _GENERIC_EVFS

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

#endif

#ifndef _EXT4_EVFS
#define _EXT4_EVFS

#ifndef EXT4_NAME_LEN
#define EXT4_NAME_LEN 255
#endif

// directory entries
struct ext4_evfs_de_add_args {
    __u64 parent_inode_number;
    __u64 child_inode_number;
    __u8 file_type;
    char name[EXT4_NAME_LEN];	// EXT4_NAME_LEN = 255
};

struct ext4_evfs_de_read_args {
    __u64 dir_inode_number;	// inode to read dentries of
    __u32 target_dentry_index;	// ith dentry to get

    // output fields of returned dentry
    __u32 inode_number;	// inode num 
    __u8 file_type;
    __u8 name_len;
    __u16 _padding;
    char name[EXT4_NAME_LEN];
};

struct ext4_evfs_de_delete_args {
    __u64 dir_inode_number;	// inode to read dentries of
    char name[EXT4_NAME_LEN];	// name of dentry to delete
};

struct ext4_evfs_de_update_args {
    __u64 dir_inode_number;
    __u32 target_dentry_index;
    __u32 new_inode_number;
};

#define EXT4_EVFS_HELLO _IO('f', 99)
#define EXT4_EVFS_IVER _IOR('f', 100, unsigned long long)
#define EXT4_EVFS_ATTR _IOR('f', 101, struct evfs_stat)
#define EXT4_EVFS_BLK_ALLOC _IO('f', 102)
#define EXT4_EVFS_BLK_FREE _IO('f', 103)
#define EXT4_EVFS_INO_ALLOC _IO('f', 104)
#define EXT4_EVFS_INO_FREE _IO('f', 105)
#define EXT4_EVFS_DEN_ADD _IOW('f', 106, struct ext4_evfs_de_add_args)
#define EXT4_EVFS_DEN_READ _IOWR('f', 107, struct ext4_evfs_de_read_args)
#define EXT4_EVFS_DEN_DELETE _IOW('f', 108, struct ext4_evfs_de_delete_args)
#define EXT4_EVFS_DEN_UPDATE _IOW('f', 109, struct ext4_evfs_de_update_args)

#endif


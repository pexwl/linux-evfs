#include <uapi/linux/evfs.h>
#include <linux/list.h>
#include <linux/fs.h>
#include <linux/hash.h>

#ifndef _KERN_GENERIC_EVFS
#define _KERN_GENERIC_EVFS

#define TINO_HTBL_BITS 10

typedef long (* evfs_inode_alloc_gbl)(struct super_block *);
typedef long (* evfs_track_fn)(struct super_block *, struct inode *);

struct evfs_ext
{
	unsigned int lblk;
	unsigned long long pblk;
	unsigned short len;	
};

struct evfs_ht_bucket
{
	spinlock_t lock;
	struct hlist_head head;
}
___cacheline_aligned_in_smp;

struct evfs_tblk_tracker
{
	struct inode * global_inode;
};

____cacheline_aligned_in_smp;

struct evfs_tino_tracker
{
	struct evfs_ht_bucket tbl[1 << TINO_HTBL_BITS];
};

long evfs_tblk_tracker_init(struct super_block *, evfs_inode_alloc_gbl, struct evfs_tblk_tracker *);
void evfs_tblk_tracker_final(struct evfs_tblk_tracker *);
void evfs_tino_tracker_init(struct evfs_tino_tracker *);
void evfs_tino_tracker_final(struct evfs_tino_tracker *);
int evfs_tino_hold(struct evfs_tino_tracker *, struct inode *);
int evfs_tino_release(struct evfs_tino_tracker *, unsigned long);

#endif


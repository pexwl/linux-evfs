#include <linux/types.h>
#include <linux/evfs.h>
#include <linux/hashtable.h>

long evfs_tblk_reverse_mapping_add(struct evfs_tblk_tracker * tracker, struct evfs_tblk_tentry * mapping)
{
	// handle unsigned long long physical block end: `pblk` < 2^48
	unsigned long long val = mapping->pblk + (unsigned long long mapping->len);
	struct evfs_ht_bucket * bkt = &(tracker->tbl[hash_64(val, TBLK_HTBL_BITS)]);
	struct evfs_tblk_ext_entry * ext;
	
	spin_lock(&(bkt->lock));
	// first check if the entry is already in the mapping
	hlist_for_each_entry(ext, &(bkt->head), node) {
		
	}

	spin_unlock(&(bkt->lock));
}

long evfs_tblk_reverse_mapping_del(struct evfs_tblk_tracker * tracker, struct evfs_tblk_tentry * mapping)
{
	
}

long evfs_tblk_tracker_init(struct super_block * sb,
		evfs_inode_alloc_gbl alloc, struct evfs_tblk_tracker * tracker)
{
	long ret = alloc(sb);
	if (ret < 0) return ret;
	for (unsigned int i = 0; i < (1 << TBLK_HTBL_BITS); i++) {
		INIT_HLIST_HEAD(&(tracker->tbl[i]));
		spin_lock_init(&(tracker->tbl[i].lock));
    }
	return ret;
}

void evfs_tblk_tracker_final(struct evfs_tblk_tracker * tracker)
{
	iput(tracker->global_inode);
}

#include <linux/types.h>
#include <linux/evfs.h>
#include <linux/hashtable.h>

int evfs_tino_hold(struct evfs_tino_tracker * tracker, struct inode * ino)
{
	spinlock_t * blck = &(tracker->tbl[hash_min(ino->i_ino, TINO_HTBL_BITS)].lock);
	spin_lock(blck);
	if (hash_hashed(&(ino->i_evfs_hash))) {
		spin_unlock(blck);
		return -EEXIST;
	}

	hash_add(tracker->tbl, &(ino->i_evfs_hash), ino->i_ino);
	spin_unlock(blck);
	return 0;
}

int evfs_tino_release(struct evfs_tino_tracker * tracker, unsigned long iindex)
{
	struct evfs_ht_bucket * bkt = &(tracker->tbl[hash_min(iindex, TINO_HTBL_BITS)]);
	struct inode * ino;
	struct hlist_node * tmp;

	spin_lock(&(bkt->lock));
	hlist_for_each_entry_safe(ino, tmp, &(bkt->head), i_evfs_hash) {
		if (ino->i_ino == iindex) {
			hash_del(&(ino->i_evfs_hash));
			iput(ino);
			return 0;
		}
	}
	spin_unlock(&(bkt->lock));
	return -ENOENT;
}

void evfs_tino_tracker_init(struct evfs_tino_tracker * tracker)
{
	for (unsigned int i = 0; i < (1 << TINO_HTBL_BITS); i++) {
		INIT_HLIST_HEAD(&(tracker->tbl[i]));
		spin_lock_init(&(tracker->tbl[i].lock));
    }
}

void evfs_tino_tracker_final(struct evfs_tino_tracker * tracker)
{
	struct hlist_head head;
	struct inode * ino;
	struct hlist_node * tmp;

	// isolate all inodes so they're no longer shared
	for (unsigned int i = 0; i < (1 << TINO_HTBL_BITS); i++) {
		spin_lock(&(tracker->tbl[i].lock));
		hlist_for_each_entry_safe(ino, tmp, &(tracker->tbl[i].head), i_evfs_hash) {
			hlist_del_init(&(ino->i_evfs_hash));
			hlist_add_head(&(ino->i_evfs_hash), &head);
		}
		spin_unlock(&(tracker->tbl[i].lock));
	}

	hlist_for_each_entry(ino, &head, i_evfs_hash) {
		iput(ino);
	}
}


#include <linux/types.h>
#include <linux/string.h>
#include <linux/fs.h>
#include <linux/jbd2.h>
#include <linux/quotaops.h>
#include "evfs.h"
#include "ext4.h"
#include "ext4_jbd2.h"

// core evfs functions
long evfs_hello(void);
long evfs_iver(struct inode *, unsigned long long *);
long evfs_getattr(struct inode *, struct evfs_stat *); 
long evfs_block_alloc(struct inode *, struct super_block *, ext4_fsblk_t);
long evfs_inode_alloc(struct super_block *, ino_t);
long evfs_inode_free(struct super_block *, ino_t);

/**
 * @brief hello from evfs
 */
long evfs_hello()
{
	printk(KERN_INFO "Hello, bad apple!\n");
	return 0;
}

/**
 * @brief get inode version
 */
long evfs_iver(struct inode * ino, unsigned long long * ver)
{
	// print some inode info
	unsigned long long kver = atomic64_read(&ino->i_version);
	memcpy(ver, &kver, sizeof(unsigned long long));
	return 0;
}

/**
 * @brief inode statistics with version
 * This is a kernel helper and/or a function hence we don't copy to
 * user directly here
 */
long evfs_getattr(struct inode * ino, struct evfs_stat * stat)
{
	struct evfs_stat kstat;
	kstat.est_dev = ino->i_sb->s_dev;
	kstat.est_ino = ino->i_ino;
	kstat.est_mode = ino->i_mode;
	kstat.est_nlink = ino->i_nlink;
	kstat.est_uid = ino->i_uid.val;
	kstat.est_gid = ino->i_gid.val;

	if (S_ISBLK(ino->i_mode) || S_ISCHR(ino->i_mode)) {
		kstat.est_rdev = ino->i_rdev;
	}

	kstat.est_version = atomic64_read(&ino->i_version);
	kstat.est_size = ino->i_size;
	kstat.est_blksize = i_blocksize(ino);
	kstat.est_blocks = ino->i_blocks;
	kstat.est_atime = ino->__i_atime.tv_sec;
	kstat.est_atime_nsec = ino->__i_atime.tv_nsec;
	kstat.est_mtime = ino->__i_mtime.tv_sec;
	kstat.est_mtime_nsec = ino->__i_mtime.tv_nsec;
	kstat.est_ctime = ino->__i_ctime.tv_sec;
	kstat.est_ctime_nsec = ino->__i_ctime.tv_nsec;

	memcpy(stat, &kstat, sizeof(struct evfs_stat));
	return 0; 
}

/**
 * @brief journal a block allocation.
 * Note that this function alone is now creating a temporary
 * inconsistency because there's a leak block (i.e. not
 * being used by any inode)
 * 
 * WARNING: TESTING ONLY; Do NOT use this on a system fs.
 * 
 * @param ino pointer to the in-memory the file inode
 * @param sb pointer to the in-memory superblock
 * @param bindex the index of block to allocate
 * @return 0 on success otherwise error code:
 *   * -EEXIST: already allocated;
 *   * -EINVAL: fail to load group descriptor. either rcu deference fails
 *     or group index out of range;
 *   * -ERRNO: other errors from ext4 functions;
 */
long evfs_block_alloc(struct inode * ino, struct super_block * sb, ext4_fsblk_t bindex)
{
	// get the group number and block offset
	ext4_group_t group;
	ext4_grpblk_t offset;
	ext4_get_group_no_and_offset(sb, bindex, &group, &offset);
	int ret = 0, err = 0;
	struct ext4_sb_info *sbi = EXT4_SB(sb);
	struct ext4_super_block *es = sbi->s_es;

	// read the block bitmap
	struct buffer_head * bitmap_bh = ext4_read_block_bitmap(sb, group);
	if (IS_ERR(bitmap_bh)) {
		return PTR_ERR(bitmap_bh); // error from ext4_read_block_bitmap
	}

	// read the group descriptor
	struct buffer_head * grpdsc_bh;
	struct ext4_group_desc * grpdsc = ext4_get_group_desc(sb, group, &grpdsc_bh);
	if (!grpdsc) {
	brelse(bitmap_bh);
	   	return (long) -EINVAL;
	}

	handle_t * handle = ext4_journal_start_with_reserve(ino, EXT4_HT_MAP_BLOCKS, 2, 0);

	if (IS_ERR(handle)) {
		ret = PTR_ERR(handle);
		goto out;
	}

	if ((err = ext4_journal_get_write_access(handle, sb, bitmap_bh, EXT4_JTR_NONE)) < 0) {
		ret = err;
		goto stop_journal;
	}

	if ((err = ext4_journal_get_write_access(handle, sb, grpdsc_bh, EXT4_JTR_NONE)) < 0) {
		ret = err;
		goto stop_journal;
	}

	ext4_lock_group(sb, group);
	if (ext4_test_and_set_bit(offset, bitmap_bh->b_data)) {
		ret = -EEXIST;
		ext4_unlock_group(sb, group);
		goto stop_journal;
	}

	// decrease free count
	ext4_free_group_clusters_set(sb, grpdsc, EXT4_NUM_B2C(sbi,
		EXT4_C2B(sbi, ext4_free_group_clusters(sb, grpdsc)) - 1));

	if (ext4_has_group_desc_csum(sb)) {
		// initialize the block containing the inode if uninitialized
		if (grpdsc->bg_flags & cpu_to_le16(EXT4_BG_BLOCK_UNINIT)) {
			grpdsc->bg_flags &= cpu_to_le16(~EXT4_BG_BLOCK_UNINIT);
			ext4_free_group_clusters_set(sb, grpdsc, 
				ext4_free_clusters_after_init(sb, group, grpdsc));
		}

		// recompute group descriptor + bitmap checksum
		ext4_block_bitmap_csum_set(sb, grpdsc, bitmap_bh);
		ext4_group_desc_csum_set(sb, group, grpdsc);
	}

	ext4_unlock_group(sb, group);

	// update per-cpu cluster (a group of blocks) counter
	percpu_counter_sub(&sbi->s_freeclusters_counter, EXT4_NUM_B2C(sbi, 1));

	// update superblock free block counter and checksum 
	lock_buffer(sbi->s_sbh);
	ext4_free_blocks_count_set(es, ext4_free_blocks_count(es) - 1);
	ext4_superblock_csum_set(sb);
	unlock_buffer(sbi->s_sbh);

	if ((err = ext4_handle_dirty_metadata(handle, ino, grpdsc_bh)) < 0) {
		ret = err;
		goto stop_journal;
	}

	if ((err = ext4_handle_dirty_metadata(handle, NULL, bitmap_bh)) < 0) {
		ret = err;
		goto stop_journal;
	}

stop_journal:
	if ((err = ext4_journal_stop(handle)) < 0) {
		ret = err;
		goto out;
	}

out:
	brelse(bitmap_bh);
	return (long) ret;
}

/** 
 * @brief journal a inode allocation.
 * Note that this function alone is now creating a temporary
 * inconsistency because there's a leak inode (i.e. not
 * being used by any inode)
 * 
 * WARNING: TESTING ONLY; Do NOT use this on a system fs.
 * 
 * @param sb pointer to the in-memory superblock
 * @param iindex the index of inode to allocate
 * @return 0 on success otherwise error code:
 *   * -EEXIST: already allocated;
 *   * -EINVAL: fail to load group descriptor. either rcu deference fails
 *   * -ENOMEM: no enough space to allocate inode in memory
 *     or group index out of range;
 *   * -ERRNO: other errors from ext4 functions;
 */
long evfs_inode_alloc(struct super_block * sb, ino_t iindex)
{
	ext4_group_t group = (iindex - 1) / EXT4_INODES_PER_GROUP(sb);
	ino_t offset = (iindex - 1) % EXT4_INODES_PER_GROUP(sb);
	int ret = 0, err = 0;
	struct buffer_head * inode_bitmap_bh;
	struct buffer_head * block_bitmap_bh;
	struct buffer_head * grpdsc_bh;
	struct ext4_sb_info *sbi = EXT4_SB(sb);

	inode_bitmap_bh = ext4_read_inode_bitmap(sb, group);
	if (IS_ERR(inode_bitmap_bh)) {
		return PTR_ERR(inode_bitmap_bh); // error from ext4_read_inode_bitmap
	}

	block_bitmap_bh = ext4_read_block_bitmap(sb, group);
	if (IS_ERR(block_bitmap_bh)) {
		brelse(inode_bitmap_bh);
		return PTR_ERR(block_bitmap_bh); // error from ext4_read_block_bitmap
	}

	// read the group descriptor
	struct ext4_group_desc * grpdsc = ext4_get_group_desc(sb, group, &grpdsc_bh);

	if (!grpdsc) {
		ret = -EINVAL;
		goto out;
	}

	// create the inode
	struct inode * ino = new_inode(sb);
	if (!ino) {
		ret = -ENOMEM;
		goto out;
	}

	if ((err = dquot_initialize(ino))) {
		ret = err;
		goto fail_init;
	}

	handle_t * handle = ext4_journal_start_with_reserve(ino, EXT4_HT_MAP_BLOCKS, 5, 0);
   
	if (IS_ERR(handle)) {
		ret = PTR_ERR(handle);
		goto fail_init;
	}

	if ((err = ext4_journal_get_write_access(handle, sb, inode_bitmap_bh, EXT4_JTR_NONE)) < 0) {
		ret = err;
		goto stop_journal;
	}
	
	if ((err = ext4_journal_get_write_access(handle, sb, block_bitmap_bh, EXT4_JTR_NONE)) < 0) {
		ret = err;
		goto stop_journal;
	}
	
	if ((err = ext4_journal_get_write_access(handle, sb, grpdsc_bh, EXT4_JTR_NONE)) < 0) {
		ret = err;
		goto stop_journal;
	}

	ext4_lock_group(sb, group);
	if (ext4_test_and_set_bit(offset, inode_bitmap_bh->b_data)) {
		ret = -EEXIST;
		ext4_unlock_group(sb, group);
		goto stop_journal;
	}
	ext4_unlock_group(sb, group);

	struct ext4_inode_info *ei = EXT4_I(ino);

	ino->i_ino = iindex;
	ino->i_mode = S_IFREG | 0644; // use file for now, may change it later
	ino->i_blocks = 0; // optimal IO size (for stat), not the fs block size; refer to ialloc.c:1253
	simple_inode_init_ts(ino); // set time
	set_nlink(ino, 1); // since i_count will be 0 eventually, set link count to 1 to make inode persistent
	ei->i_crtime = inode_get_mtime(ino);

	memset(ei->i_data, 0, sizeof(ei->i_data));
	ei->i_dir_start_lookup = 0;
	ei->i_disksize = 0;
	ei->i_flags = 0;
	ei->i_file_acl = 0;
	ei->i_dtime = 0;
	ei->i_block_group = group;
	ei->i_last_alloc_group = ~0;

	ext4_set_inode_flags(ino, true);

	// you must locked a dangling inode here
	if (insert_inode_locked(ino) < 0) {
		ret = -EIO;
		goto stop_journal;
	}

	ino->i_generation = get_random_u32();

	if (ext4_has_metadata_csum(sb)) {
		__le32 inum = cpu_to_le32(ino->i_ino);
		__le32 gen = cpu_to_le32(ino->i_generation);
		__u32 csum = ext4_chksum(sbi, sbi->s_csum_seed, (__u8 *) &inum, sizeof(inum));
		ei->i_csum_seed = ext4_chksum(sbi, csum, (__u8 *) &gen, sizeof(gen));
	}

	ext4_clear_state_flags(ei); /* only relevant on 32-bit archs */
	ext4_set_inode_state(ino, EXT4_STATE_NEW);

	// allocate quota for inode
	if ((err = dquot_alloc_inode(ino))) {
		ret = err;
		goto fail_drop;
	}

	// note that ext4_mark_inode_dirty get write access to journal, 
	// mark journal dirty implicitly, so we do NOT need it here
	if ((err = ext4_mark_inode_dirty(handle, ino))) {
		ret = err;
		goto fail_free_drop;
	}

	// update the metadata in the block group descriptor
	ext4_lock_group(sb, group);
	// modify the free inode count
	ext4_free_inodes_set(sb, grpdsc, ext4_free_inodes_count(sb, grpdsc) - 1);

	if (ext4_has_group_desc_csum(sb)) {
		// initialize the block containing the inode if uninitialized
		if (grpdsc->bg_flags & cpu_to_le16(EXT4_BG_BLOCK_UNINIT)) {
			grpdsc->bg_flags &= cpu_to_le16(~EXT4_BG_BLOCK_UNINIT);
			ext4_free_group_clusters_set(sb, grpdsc,
				ext4_free_clusters_after_init(sb, group, grpdsc));
			ext4_block_bitmap_csum_set(sb, grpdsc, block_bitmap_bh);
		}

		ext4_grpblk_t free = EXT4_INODES_PER_GROUP(sb) - ext4_itable_unused_count(sb, grpdsc);
		if (grpdsc->bg_flags & cpu_to_le16(EXT4_BG_INODE_UNINIT)) {
			grpdsc->bg_flags &= cpu_to_le16(~EXT4_BG_INODE_UNINIT);
			free = 0;
		}

		if (offset > free) {
			ext4_itable_unused_set(sb, grpdsc, (EXT4_INODES_PER_GROUP(sb) - offset - 1));
		}

		// recompute group descriptor + bitmap checksum
		ext4_inode_bitmap_csum_set(sb, grpdsc, inode_bitmap_bh, EXT4_INODES_PER_GROUP(sb) / 8);
		ext4_group_desc_csum_set(sb, group, grpdsc);
	}

	ext4_unlock_group(sb, group);

	// decrease free inode count
	percpu_counter_dec(&sbi->s_freeinodes_counter);
	// DONE: there is no superblock free inode counters, so we don't update them

	if ((err = ext4_handle_dirty_metadata(handle, ino, grpdsc_bh)) < 0) {
		ret = err;
		goto fail_free_drop;
	}

	if ((err = ext4_handle_dirty_metadata(handle, NULL, block_bitmap_bh)) < 0) {
		ret = err;
		goto fail_free_drop;
	}

	if ((err = ext4_handle_dirty_metadata(handle, NULL, inode_bitmap_bh)) < 0) {
		ret = err;
		goto fail_free_drop;
	}

	unlock_new_inode(ino);
	printk(KERN_INFO "[INODE_ALLOC] i_count = %d\n", atomic_read(&ino->i_count));
	printk(KERN_INFO "[INODE_ALLOC] group = %u\n", group);
	printk(KERN_INFO "[INODE_ALLOC] offset = %lu\n", offset);
	goto stop_journal;

fail_free_drop:
	dquot_free_inode(ino);

fail_drop:
	clear_nlink(ino);
	unlock_new_inode(ino);

stop_journal:
	if ((err = ext4_journal_stop(handle)) < 0) ret = err;
	if (ret >= 0) {
		iput(ino);
		goto out;
	}

fail_init:	
	dquot_drop(ino);
	ino->i_flags |= S_NOQUOTA;
	iput(ino);

out:
	brelse(block_bitmap_bh);
	brelse(inode_bitmap_bh);
	return (long) ret;
}

long evfs_inode_free(struct super_block * sb, ino_t iindex)
{
	ext4_group_t group = (iindex - 1) / EXT4_INODES_PER_GROUP(sb);
	ino_t offset = (iindex - 1) % EXT4_INODES_PER_GROUP(sb);
	int ret = 0, err = 0;

	// get the inode
	struct inode * ino = ext4_iget(sb, iindex, EXT4_IGET_NORMAL);
	if (IS_ERR(ino)) {
		ret = PTR_ERR(ino);
		goto out;
	}

	if (ino->i_nlink > 0) {
		ret = -EINVAL;
		goto out;
	}

	handle_t * handle = ext4_journal_start_with_reserve(ino, EXT4_HT_MAP_BLOCKS, 3, 0);
   
	if (IS_ERR(handle)) {
		ret = PTR_ERR(handle);
		goto out;
	}

	ino->i_size = 0;
	// TODO: may need to delete inode from the orphan list
	EXT4_I(ino)->i_dtime = (__u32) ktime_get_real_seconds();

	if ((err = ext4_mark_inode_dirty(handle, ino)) < 0) {
		ret = err;
		goto out;
	}

	printk(KERN_INFO "[INODE_FREE] i_count = %d\n", atomic_read(&ino->i_count));
	printk(KERN_INFO "[INODE_FREE] group = %u\n", group);
	printk(KERN_INFO "[INODE_FREE] offset = %lu\n", offset);
	goto stop_journal;

stop_journal:
	if ((err = ext4_journal_stop(handle)) < 0) ret = err;

out:
	iput(ino); // release the i_count and let vfs finalize
	return (long) ret;
}

long ext4_ioctl_evfs(unsigned int cmd, struct inode * ino, struct super_block * sb, unsigned long arg)
{
	switch(cmd) {
		case EXT4_IOC_EVFS_HELLO:
			return evfs_hello();
		case EXT4_IOC_EVFS_IVER:
			unsigned long long uver;
			size_t llu_size = sizeof(unsigned long long);
			evfs_iver(ino, &uver);
			if (copy_to_user((void *) arg, &uver, llu_size)) {
				return -EFAULT;
			}
			return 0;
		case EXT4_IOC_EVFS_GETATTR:
			struct evfs_stat ustat;
			size_t evfs_stat_size = sizeof(struct evfs_stat);
			evfs_getattr(ino, &ustat);
			if (copy_to_user((void *) arg, &ustat, evfs_stat_size)) {
				return -EFAULT;
			}
			return 0;
		case EXT4_IOC_EVFS_BLK_ALLOC:
			return evfs_block_alloc(ino, sb, (ext4_fsblk_t) arg);
		case EXT4_IOC_EVFS_INO_ALLOC:
			return evfs_inode_alloc(sb, (ino_t) arg);
		case EXT4_IOC_EVFS_INO_FREE:
			return evfs_inode_free(sb, (ino_t) arg);
		default:
			return -ENOTTY;
	}
}

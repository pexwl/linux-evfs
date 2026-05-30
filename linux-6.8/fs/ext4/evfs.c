#include <linux/types.h>
#include <linux/string.h>
#include <linux/fs.h>
#include <linux/jbd2.h>
#include <linux/quotaops.h>
#include "evfs.h"
#include "ext4.h"
#include "ext4_jbd2.h"
#include "ext4_extents.h"
#include "../internal.h"

// core evfs functions
long evfs_hello(void);
long evfs_iver(struct inode *, unsigned long long *);
long evfs_block_alloc(struct inode *, struct super_block *, ext4_fsblk_t);
long evfs_block_free(struct inode *, struct super_block *, ext4_fsblk_t);
long evfs_fspace_iter(struct super_block *, struct ext4_evfs_fsp_iter_args *);
long evfs_inode_read(struct super_block *, struct evfs_ino_read_args *); 
long evfs_inode_alloc(struct super_block *, ino_t);
long evfs_inode_free(struct super_block *, ino_t);
long evfs_inode_iter(struct super_block *, struct ext4_evfs_ino_iter_args *);
long evfs_inode_remap(struct super_block *, struct ext4_evfs_ino_rmp_args *);
long evfs_dentry_add(struct super_block *, struct ext4_evfs_de_add_args *);
long evfs_dentry_read(struct super_block *, struct ext4_evfs_de_read_args *);
long evfs_dentry_delete(struct super_block *, struct ext4_evfs_de_delete_args *);
long evfs_dentry_update(struct super_block *, struct ext4_evfs_de_update_args *);
long evfs_extent_read(struct super_block *, struct ext4_evfs_ext_read_args *);

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

long evfs_block_free(struct inode * ino, struct super_block * sb, ext4_fsblk_t bindex)
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
	if (!ext4_test_and_clear_bit(offset, bitmap_bh->b_data)) {
		ret = -EEXIST;
		ext4_unlock_group(sb, group);
		goto stop_journal;
	}

	// increase free count
	ext4_free_group_clusters_set(sb, grpdsc, EXT4_NUM_B2C(sbi,
		EXT4_C2B(sbi, ext4_free_group_clusters(sb, grpdsc)) + 1));

	if (ext4_has_group_desc_csum(sb)) {
		// recompute group descriptor + bitmap checksum
		ext4_block_bitmap_csum_set(sb, grpdsc, bitmap_bh);
		ext4_group_desc_csum_set(sb, group, grpdsc);
	}

	ext4_unlock_group(sb, group);

	// update per-cpu cluster (a group of blocks) counter
	percpu_counter_add(&sbi->s_freeclusters_counter, EXT4_NUM_B2C(sbi, 1));

	// update superblock free block counter and checksum 
	lock_buffer(sbi->s_sbh);
	ext4_free_blocks_count_set(es, ext4_free_blocks_count(es) + 1);
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
 * @brief inode statistics with version
 * This is a kernel helper and/or a function hence we don't copy to
 * user directly here
 */
long evfs_inode_read(struct super_block * sb, struct evfs_ino_read_args * args)
{
	int ret = 0;
	struct inode * ino = ext4_iget(sb, args->in.iindex, EXT4_IGET_NORMAL);
	if (IS_ERR(ino)) {
		ret = PTR_ERR(ino);
		goto out;
	}

	inode_lock_shared(ino);

	evfs_stat stat;
	stat.est_dev = ino->i_sb->s_dev;
	stat.est_ino = ino->i_ino;
	stat.est_mode = ino->i_mode;
	stat.est_nlink = ino->i_nlink;
	stat.est_uid = ino->i_uid.val;
	stat.est_gid = ino->i_gid.val;

	if (S_ISBLK(ino->i_mode) || S_ISCHR(ino->i_mode)) {
		stat.est_rdev = ino->i_rdev;
	}

	stat.est_version = atomic64_read(&ino->i_version);
	stat.est_size = ino->i_size;
	stat.est_blksize = i_blocksize(ino);
	stat.est_blocks = ino->i_blocks;
	stat.est_atime = ino->__i_atime.tv_sec;
	stat.est_atime_nsec = ino->__i_atime.tv_nsec;
	stat.est_mtime = ino->__i_mtime.tv_sec;
	stat.est_mtime_nsec = ino->__i_mtime.tv_nsec;
	stat.est_ctime = ino->__i_ctime.tv_sec;
	stat.est_ctime_nsec = ino->__i_ctime.tv_nsec;

	inode_unlock_shared(ino);

	memcpy(&(args->out), &stat, sizeof(evfs_stat));
	iput(ino);

out:
	return (long) ret; 
}

long evfs_fspace_iter(struct super_block * sb, struct ext4_evfs_fsp_iter_args * args)
{
	__u64 found_start = 0;
	__u64 found_length = 0;
	__u64 total_blocks = ext4_blocks_count(EXT4_SB(sb)->s_es);
	int in_free_extent = 0;

	for (__u64 b = args->in.start; b <= total_blocks; b++) {
		ext4_group_t group;
		ext4_grpblk_t offset;
		ext4_get_group_no_and_offset(sb, b, &group, &offset);

		struct buffer_head *bitmap_bh = ext4_read_block_bitmap(sb, group);
		// found unreadable blocks
		if (IS_ERR_OR_NULL(bitmap_bh)) {
			// assume current free extent stops right before unreadable blocks
			if (in_free_extent) break;
			// otherwise, continue checking for free extents after unreadable blocks
			continue;
		}

		int is_free = !ext4_test_bit(offset, bitmap_bh->b_data);
		brelse(bitmap_bh);

		if (is_free) {
			if (!in_free_extent) {
				found_start = b;
				in_free_extent = 1;
			}
			found_length++;
		} else if (in_free_extent) {
			break;
		}
	}

	args->out.block = found_start;
	args->out.length = found_length;
	return 0;
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
	// so we do NOT need it here
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

/**
 * @brief help to free an inode
 * Note that this function is only intended to free an inode
 * that is created using evfs_inode_alloc but is never modified
 */
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

	// force link count to 0 and let VFS evict it
	set_nlink(ino, 0);
	handle_t * handle = ext4_journal_start_with_reserve(ino, EXT4_HT_MAP_BLOCKS, 3, 0);

	if (IS_ERR(handle)) {
		ret = PTR_ERR(handle);
		goto out;
	}

	ino->i_size = 0;
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

long evfs_inode_iter(struct super_block * sb, struct ext4_evfs_ino_iter_args * args)
{
	__u32 found = 0;
	__u32 total = le32_to_cpu(EXT4_SB(sb)->s_es->s_inodes_count);

	for (__u32 i = args->in.start; i <= total; i++) {
		// compute the group and offset of the current inode
		// inodes are 1-indexed
		ext4_group_t group = (i - 1) / EXT4_INODES_PER_GROUP(sb);
		ext4_grpblk_t offset = (i - 1) % EXT4_INODES_PER_GROUP(sb);

		// read inode bitmap for this group
		struct buffer_head *bitmap_bh = ext4_read_inode_bitmap(sb, group);
		if (IS_ERR_OR_NULL(bitmap_bh)) {
			continue;
		}

		int in_use = ext4_test_bit(offset, bitmap_bh->b_data);
		brelse(bitmap_bh);

		if (in_use) {
			found = i;
			break;
		}
	}

	args->out.ino_num = found;
	return 0;
}

long evfs_inode_remap(struct super_block * sb, struct ext4_evfs_ino_rmp_args * args)
{	
	struct inode *target_inode = NULL;
	bool inode_locked = false;
	bool ext_tree_locked = false;
	handle_t *handle = NULL;
	int err = 0;

	/* User array of new extents */
	if (args->in.num_exts == 0 || args->in.num_exts > MAX_EXT4_EXTENTS) {
		err = -EINVAL;
		pr_warn("evfs: Invalid number of extents: %d\n", args->in.num_exts);
		goto inode_remap_out;
	}

	target_inode = ext4_iget(sb, args->in.ino_num, EXT4_IGET_NORMAL); 
	if (IS_ERR(target_inode)) {
		err = PTR_ERR(target_inode);
		goto inode_remap_out;
	}

	if (!ext4_test_inode_flag(target_inode, EXT4_INODE_EXTENTS)) {
		err = -EOPNOTSUPP;
		goto inode_remap_out;
	}

	/* Affect up to 10 blocks */
	handle = ext4_journal_start(target_inode, EXT4_HT_MISC, 10);
	if (IS_ERR(handle)) {
		err = PTR_ERR(handle);
		goto inode_remap_out;
	}

	inode_lock(target_inode);	/* lock inode */
	inode_locked = true;
	down_write(&EXT4_I(target_inode)->i_data_sem);	/* lock extent tree with rw semaphore */
	ext_tree_locked = true;

	/* Prevent new writes during page invalidation */
	filemap_write_and_wait(&target_inode->i_data);

	/* Invalidate stale page cache */
	invalidate_inode_pages2(target_inode->i_mapping);

	/* Remove all existing extents */
	err = ext4_ext_remove_space(target_inode, 0, EXT_MAX_BLOCKS - 1);
	if (err) {
		pr_warn("evfs: ext4_ext_remove_space failed: %d\n", err);
		goto inode_remap_out;
	}

	/* Inode removed of its data; set size to 0 */
	target_inode->i_size = 0;

	ext4_lblk_t logical_block = 0;	/* Logical block that each new extent starts from */
	for (int i = 0; i < args->in.num_exts; i++) {
		/* Insert new extent at logical block 0 */
		struct ext4_extent new_extent;
		new_extent.ee_block = cpu_to_le32(logical_block);
		new_extent.ee_len = cpu_to_le16(args->in.exts[i].length);
		new_extent.ee_start_hi = cpu_to_le16(args->in.exts[i].start >> 32);
		new_extent.ee_start_lo = cpu_to_le32(args->in.exts[i].start & 0xffffffffULL);

		struct ext4_ext_path *path = NULL;
		path = ext4_find_extent(target_inode, logical_block, &path, 0);
		if (IS_ERR(path)) {
			err = PTR_ERR(path);
			path = NULL;
			goto inode_remap_out;
		}

		err = ext4_ext_insert_extent(handle, target_inode, &path, &new_extent, 0);
		if (path) {
			ext4_ext_drop_refs(path);
			kfree(path);
		}
		if (err) {
			pr_warn("evfs: ext4_ext_insert_extent failed: %d\n", err);
			goto inode_remap_out;
		}

		/* update inode size and mark dirty */
		target_inode->i_size += (loff_t)(args->in.exts[i].length) * sb->s_blocksize;

		/* Update starting logical block for the next extent */
		logical_block += args->in.exts[i].length;
	}
	
	err = ext4_mark_inode_dirty(handle, target_inode);
	if (err) {
		goto inode_remap_out;
	}

	ext4_journal_stop(handle);
	handle = NULL;  /* already stopped, don't stop again in cleanup */
	write_inode_now(target_inode, 1);  /* 1 = wait for completion */

inode_remap_out:
	if (!IS_ERR_OR_NULL(target_inode)) {
		if (ext_tree_locked) up_write(&EXT4_I(target_inode)->i_data_sem);
		if (inode_locked) inode_unlock(target_inode);
	}
	if (!IS_ERR_OR_NULL(handle)) {
		ext4_journal_stop(handle);
	}
	if (!IS_ERR_OR_NULL(target_inode)) {
		iput(target_inode);
	}
	return err;
}

long evfs_dentry_add(struct super_block * sb, struct ext4_evfs_de_add_args * args)
{
	pr_info("ext4: ADD_DENTRY called\n");
	struct inode * dir_inode;
	struct inode * child_inode;
	struct dentry * parent_dentry;
	struct dentry * new_dentry;
	struct qstr qname;
	int err;

	size_t name_len = strlen(args->in.name);
	if (name_len == 0 || name_len > EXT4_NAME_LEN) {
		pr_warn("ext4-evfs: invalid name length %zu\n", name_len);
		return -EINVAL;
	}

	// get parent directory inode
	dir_inode = ext4_iget(sb, args->in.parent_ino_num, EXT4_IGET_NORMAL);
	if (IS_ERR(dir_inode)) {
		return PTR_ERR(dir_inode);
	}

	inode_lock(dir_inode);

	// ensure parent is a dir
	if (!S_ISDIR(dir_inode->i_mode)) {
		pr_warn("ext4-evfs: parent inode %lu is not a directory\n", args->in.parent_ino_num);
		inode_unlock(dir_inode);
		iput(dir_inode);
		return -ENOTDIR;
	}

	// get child inode (this must exist)
	child_inode = ext4_iget(sb, args->in.child_ino_num, EXT4_IGET_NORMAL);
	if (IS_ERR(child_inode)) {
		inode_unlock(dir_inode);
		iput(dir_inode);
		return PTR_ERR(child_inode);
	}

	// look up parent dentry
	parent_dentry = d_find_any_alias(dir_inode);
	if (!parent_dentry) {
		inode_unlock(dir_inode);
		iput(child_inode);
		iput(dir_inode);
		return -ENOENT;
	}

	// create dentry for new entry
	qname.name = args->in.name;
	qname.len = strlen(args->in.name);
	qname.hash = full_name_hash(parent_dentry, qname.name, qname.len);

	if (unlikely(IS_CASEFOLDED(dir_inode))) {
		sb->s_d_op->d_hash(parent_dentry, &qname);
	}

	new_dentry = d_alloc(parent_dentry, &qname);
	if (!new_dentry) {
		inode_unlock(dir_inode);
		dput(parent_dentry);
		iput(child_inode);
		iput(dir_inode);
		return -ENOMEM;
	}

	// TODO: add `dquot_initialize(dir);` refer to fs/ext4/namei.c:3513
	inode_lock(child_inode);
	err = __ext4_link(dir_inode, child_inode, new_dentry);
	inode_unlock(child_inode);
	inode_unlock(dir_inode);

	// cleanup
	dput(new_dentry);
	dput(parent_dentry);
	iput(child_inode);
	iput(dir_inode);

	if (err) {
		pr_warn("ext4-evfs: __ext4_link failed: %d\n", err);
		return (long) err;
	}

	pr_info("ext4-evfs: added entry '%s' (parent=%lu, child=%lu)\n",
			args->in.name, args->in.parent_ino_num, args->in.child_ino_num);

	return 0;
}

long evfs_dentry_read(struct super_block * sb, struct ext4_evfs_de_read_args * args)
{
	pr_info("ext4: READ_DENTRY called\n");
	struct inode * dir_inode;
	struct buffer_head * bh = NULL;
	struct ext4_dir_entry_2 * curr_dentry;
	unsigned int offset = 0;
	unsigned int blocksize;
	unsigned int entry_count = 0;
	int err = 0;

	// get directory inode
	dir_inode = ext4_iget(sb, args->in.dir_ino_num, EXT4_IGET_NORMAL);
	if (IS_ERR(dir_inode)) {
		err = PTR_ERR(dir_inode);
		pr_warn("ext4-evfs: failed to get directory inode: %lu: %d\n",
				args->in.dir_ino_num, err);
		goto out;
	}

	inode_lock_shared(dir_inode);

	// verify it is a directory
	if (!(S_ISDIR(dir_inode->i_mode))) {
		inode_unlock_shared(dir_inode);
		err = -ENOTDIR;
		pr_warn("ext4-evfs: inode %lu is not a directory\n", args->in.dir_ino_num);
		goto dir_check_fail;
	}

	blocksize = dir_inode->i_sb->s_blocksize;

	// read first directory block
	bh = ext4_bread(NULL, dir_inode, 0, 0);
	if (IS_ERR_OR_NULL(bh)) {
		inode_unlock_shared(dir_inode);
		err = bh ? PTR_ERR(bh) : -EIO;
		pr_warn("ext4-evfs: failed to read directory block: %d\n", err);
		goto dir_check_fail;
	}

	curr_dentry = (struct ext4_dir_entry_2 *) bh->b_data;

	// iterate thru dentries till we find the specified one
	unsigned int trailing_checksum_size = 0; // space reserved at the end of the block for the checksum
	if (ext4_has_metadata_csum(sb)) {	// check if this fs is using metadata checksums
		trailing_checksum_size = sizeof(struct ext4_dir_entry_tail);
	}

	while (offset < blocksize - trailing_checksum_size) {

		unsigned int rec_len = le16_to_cpu(curr_dentry->rec_len);

		if (rec_len == 0) {	// end of entries
			break;
		}

		if (curr_dentry->inode != 0) {	// skip deleted entries
			if (entry_count == args->in.target_dentry_index) {	// found specified dentry
				// copy info of this found dentry
				args->out.ino_num = le32_to_cpu(curr_dentry->inode);
				args->out.file_type = curr_dentry->file_type;
				args->out.name_len = curr_dentry->name_len;
				memcpy(args->out.name, curr_dentry->name, curr_dentry->name_len);
				args->out.name[EXT4_NAME_LEN - 1] = 0;
				inode_unlock_shared(dir_inode);
				goto release;
			}

			entry_count++;
		}

		offset += rec_len;
		curr_dentry = (struct ext4_dir_entry_2 *)((char *)curr_dentry + rec_len);

	}

	// didn't find specified inode
	inode_unlock_shared(dir_inode);
	args->out.ino_num = 0;
	args->out.file_type = 0;
	args->out.name_len = 0;
	args->out.name[0] = 0;

release:
	brelse(bh);
dir_check_fail:
	iput(dir_inode);
out:
	return (long) err;
}

long evfs_dentry_delete(struct super_block * sb, struct ext4_evfs_de_delete_args * args)
{
	// TODO: suggestion: use `lookup_one_len` to replace finding 
	// TODO: 	     inode manually to improve maintainability
	pr_info("ext4: DELETE_DENTRY called\n");

	struct buffer_head * bh = NULL;
	struct inode * dir_inode;
	struct inode * child_inode = NULL;
	struct dentry * parent_dentry;
	struct dentry * target_dentry;
	struct qstr qname;
	struct ext4_dir_entry_2 * de;	
	int err = 0;
	bool parent_dput_called = false;
	bool target_dput_called = false;

	// get directory inode
	dir_inode = ext4_iget(sb, args->in.dir_ino_num, EXT4_IGET_NORMAL);
	if (IS_ERR(dir_inode)) {
		err = PTR_ERR(dir_inode);
		pr_warn("ext4-evfs: failed to get directory inode: %lu: %d\n",
				args->in.dir_ino_num, err);
		goto out;
	}

	// lock the parent inode
	inode_lock(dir_inode);

	// verify it is a directory
	if (!(S_ISDIR(dir_inode->i_mode))) {
		pr_warn("ext4-evfs: inode %lu is not a directory\n", args->in.dir_ino_num);
		err = -ENOTDIR;
		goto release_pino;
	}

	// look up parent dentry
	parent_dentry = d_find_any_alias(dir_inode);
	if (!parent_dentry) {
		err = -ENOENT;
		goto release_pino;
	}

	qname.name = args->in.name;
	qname.len = strlen(args->in.name);
	qname.hash = full_name_hash(parent_dentry, qname.name, qname.len);

	if (unlikely(IS_CASEFOLDED(dir_inode))) {
		sb->s_d_op->d_hash(parent_dentry, &qname);
	}

	bh = ext4_find_entry(dir_inode, &qname, &de, NULL);
	if (IS_ERR_OR_NULL(bh)) {
		pr_warn("ext4-evfs: entry '%s' not found\n", qname.name);
		err = bh ? PTR_ERR(bh) : -ENOENT;
		goto release_pde;
	}

	// get child inode
	uint32_t child_ino_num = le32_to_cpu(de->inode);
	brelse(bh);
	child_inode = ext4_iget(sb, child_ino_num, EXT4_IGET_NORMAL);

	if (IS_ERR(child_inode)) {
		pr_warn("ext4-evfs: failed to get child inode: %d\n", err);
		err = PTR_ERR(child_inode);
		goto release_pde;
	}

	inode_lock(child_inode);
	target_dentry = d_alloc(parent_dentry, &qname);
	if (!target_dentry) {
		err = -ENOMEM;
		goto release_cino;
	}

	d_add(target_dentry, child_inode);
	printk(KERN_INFO "ext4-evfs: inode state %lx\n", child_inode->i_state);
	// TODO: add `dquot_initialize(dir);` refer to fs/ext4/namei.c:3316
	// TODO: add `dquot_initialize(d_inode(dentry));` refer to fs/ext4/namei.c:3319
	err = __ext4_unlink(dir_inode, &qname, child_inode, target_dentry);

	// cleanup
	dput(target_dentry);
	target_dput_called = true;

	if (err) {
		pr_warn("ext4-evfs: __ext4_unlink failed: %d\n", err);
	} else {
		pr_info("ext4-evfs: deleted entry '%s' (inode number=%u) from parent=%lu\n",
				args->in.name, child_ino_num, args->in.dir_ino_num);
	}

release_cino:
	inode_unlock(child_inode);
	if (!target_dput_called) iput(child_inode);

release_pde:
	dput(parent_dentry);
	parent_dput_called = true;	

release_pino:
	inode_unlock(dir_inode);
	if (!parent_dput_called) iput(dir_inode);

out:
	return (long) err;
}

long evfs_dentry_update(struct super_block * sb, struct ext4_evfs_de_update_args * args)
{
	pr_info("ext4: UPDATE_DENTRY called\n");

	struct inode * dir_inode;
	struct buffer_head * bh = NULL;
	struct ext4_dir_entry_2 * curr_dentry;
	handle_t * handle;
	unsigned int offset = 0;
	unsigned int blocksize;
	unsigned int entry_count = 0;
	int err = -ENOENT;

	// get directory inode
	dir_inode = ext4_iget(sb, args->in.dir_ino_num, EXT4_IGET_NORMAL);
	if (IS_ERR(dir_inode)) {
		err = PTR_ERR(dir_inode);
		pr_warn("ext4-evfs: failed to get directory inode: %lu: %d\n",
				args->in.dir_ino_num, err);
		return err;
	}

	// verify it is a directory
	if (!(S_ISDIR(dir_inode->i_mode))) {
		pr_warn("ext4-evfs: inode %lu is not a directory\n", args->in.dir_ino_num);
		iput(dir_inode);
		return -ENOTDIR;
	}

	blocksize = dir_inode->i_sb->s_blocksize;
	// start journal transaction
	handle = ext4_journal_start(dir_inode, EXT4_HT_DIR, 1);
	if (IS_ERR(handle)) {
		err = PTR_ERR(handle);
		iput(dir_inode);
		return err;
	}

	// read first directory block
	bh = ext4_bread(NULL, dir_inode, 0, 0);
	if (IS_ERR_OR_NULL(bh)) {
		err = bh ? PTR_ERR(bh) : -EIO;
		pr_warn("ext4-evfs: failed to read directory block:%d\n", err);
		goto update_dentry_out_stop;
	}

	// get write access to directory block
	err = ext4_journal_get_write_access(handle, sb, bh, EXT4_JTR_NONE);
	if (err) {
		pr_warn("ext4-evfs: failed to get write access to journal: %d\n", err);
		goto update_dentry_out_brelse;
	}

	curr_dentry = (struct ext4_dir_entry_2 *) bh->b_data;

	// iterate through to find the ith dentry
	unsigned int trailing_checksum_size = 0; // space reserved at the end of the block for the checksum
	if (ext4_has_metadata_csum(sb)) {	// check if this fs is using metadata checksums
		trailing_checksum_size = sizeof(struct ext4_dir_entry_tail);
	}

	while (offset < blocksize - trailing_checksum_size) {

		unsigned int rec_len = le16_to_cpu(curr_dentry->rec_len);

		if (rec_len == 0) {	// end of entries
			break;
		}

		if (curr_dentry->inode != 0) {	// skip deleted entries
			// found target dentry
			if (entry_count == args->in.target_dentry_index) {	
				curr_dentry->inode = cpu_to_le32(args->in.new_ino_num);

				// mark buffer dirty (handles dirblock checksum)
				err = ext4_handle_dirty_dirblock(handle, dir_inode, bh);

				pr_info("ext4-evfs: updated entry %u to inode %u\n",
						entry_count, curr_dentry->inode);

				goto update_dentry_out_brelse;
			}

			entry_count++;
		}

		offset += rec_len;
		curr_dentry = (struct ext4_dir_entry_2 *)((char *) curr_dentry + rec_len);

	}

	if (err == -ENOENT) {
		pr_warn("ext4-evfs: update failed, index %u out of bounds\n",
				args->in.target_dentry_index);
	}

update_dentry_out_brelse:
	brelse(bh);
update_dentry_out_stop:
	ext4_journal_stop(handle);
	iput(dir_inode);
	return (long) err;
}

long evfs_extent_read(struct super_block * sb, struct ext4_evfs_ext_read_args * args)
{
	struct ext4_ext_path *path = NULL;
	__u32 count = 0;
	ext4_lblk_t block = 0;
	bool islocked_extent_tree = false;
	int err = 0;

	// get target inode
	struct inode * inode = ext4_iget(sb, args->in.ino_num, EXT4_IGET_NORMAL);
	if (IS_ERR(inode)) {
		return PTR_ERR(inode);
	}

	// need to ensure inode is extent-based rather than block-based (older)
	if (!ext4_test_inode_flag(inode, EXT4_INODE_EXTENTS)) {
		iput(inode);
		return -EOPNOTSUPP;
	}

	/* Lock extent tree */
	down_read(&EXT4_I(inode)->i_data_sem);
	islocked_extent_tree = true;

	while (count < args->in.max_num_exts) {
		// find which extent covers <block>, or the next that follows it	
		// each inode uses an extent tree. Extent is stored at leaf. Path[-1] is leaf
		path = ext4_find_extent(inode, block, &path, 0);
		if (IS_ERR(path)) {
			path = NULL;
			break;
		}

		/* Find depth of extent tree */
		int depth = ext_depth(inode);
		/* path[depth] is leaf of extree */
		struct ext4_extent *ex = path[depth].p_ext;	/* p_ext points to extent */
		if (!ex) {
			break;	/* no extents */
		}
		/* Check if we are looking at the same extent over and over, it means there's no more */
		if (le32_to_cpu(ex->ee_block) + ext4_ext_get_actual_len(ex) <= block) {
			break;
		}

		// construct our custom extent struct
		args->out.exts[count].start = ((ext4_fsblk_t)le16_to_cpu(ex->ee_start_hi) << 32) |
					le32_to_cpu(ex->ee_start_lo);
		args->out.exts[count].length = ext4_ext_get_actual_len(ex);
		count++;

		/* 
		Advance block cursor to just past this extent 
		ext4_find_extent (on next iteration) will find the next extent at or past this block
		*/ 
		block = le32_to_cpu(ex->ee_block) + ext4_ext_get_actual_len(ex);
		// pr_info("evfs: reassigned block to %u\n", block);
		// // drop buffer head refs but keep path allocated for reuse
		// ext4_ext_drop_refs(path);
	}


	if (islocked_extent_tree) up_read(&EXT4_I(inode)->i_data_sem);

	if (path) {
		ext4_ext_drop_refs(path);
		kfree(path);
	}

	if (inode) iput(inode);

	args->out.num_exts = count;
	return err;
}

long ext4_ioctl_evfs(unsigned int cmd, struct inode * ino, struct super_block * sb, unsigned long arg)
{
	long ret;
	switch(cmd) {
		case EXT4_EVFS_HELLO:
			return evfs_hello();
		case EXT4_EVFS_IVER:
			unsigned long long u_ver;
			ret = evfs_iver(ino, &u_ver);
			if (copy_to_user((void __user *) arg, &u_ver, sizeof(u_ver)))
				return -EFAULT;

			return ret;
		case EXT4_EVFS_BLK_ALLOC:
			return evfs_block_alloc(ino, sb, (ext4_fsblk_t) arg);
		case EXT4_EVFS_BLK_FREE:
			return evfs_block_free(ino, sb, (ext4_fsblk_t) arg);
		case EXT4_EVFS_INO_ALLOC:
			return evfs_inode_alloc(sb, (ino_t) arg);
		case EXT4_EVFS_INO_FREE:
			return evfs_inode_free(sb, (ino_t) arg);
		case EXT4_EVFS_INO_READ:
		{
			struct evfs_ino_read_args k_args;
			struct evfs_ino_read_args __user * u_args =
				(struct evfs_ino_read_args __user *) arg;

			if (copy_from_user(&(k_args.in), &(u_args->in),
						sizeof(k_args.in)))
				return -EFAULT;
			
			ret = evfs_inode_read(sb, &k_args);
			if (copy_to_user(&(u_args->out), &(k_args.out),
						sizeof(k_args.out)))
				return -EFAULT;

			return ret;
		}
		case EXT4_EVFS_INO_RMP:
		{
			struct ext4_evfs_ino_rmp_args k_args;
			struct ext4_evfs_ino_rmp_args __user * u_args =
				(struct ext4_evfs_ino_rmp_args __user *) arg;
			if (copy_from_user(&(k_args.in), &(u_args->in), sizeof(k_args.in)))
				return -EFAULT;
			
			size_t ext_arr_size = sizeof(struct ext4_evfs_ext) * u_args->in.num_exts;
			k_args.in.exts = (struct ext4_evfs_ext *) kmalloc(ext_arr_size, GFP_KERNEL);
			if (copy_from_user(k_args.in.exts, u_args->in.exts, ext_arr_size))
				return -EFAULT;

			return evfs_inode_remap(sb, &k_args);
		}
		case EXT4_EVFS_DEN_ADD:
		{
			struct ext4_evfs_de_add_args k_args;
			struct ext4_evfs_de_add_args __user * u_args =
				(struct ext4_evfs_de_add_args __user *) arg;

			if (copy_from_user(&(k_args.in), &(u_args->in), sizeof(k_args.in)))
				return -EFAULT;

			k_args.in.name[sizeof(k_args.in.name) - 1] = 0;
			return evfs_dentry_add(sb, &k_args);
		}
		case EXT4_EVFS_DEN_READ:
		{
			struct ext4_evfs_de_read_args k_args;
			struct ext4_evfs_de_read_args __user * u_args =
				(struct ext4_evfs_de_read_args __user *) arg;

			if (copy_from_user(&(k_args.in), &(u_args->in),
						sizeof(k_args.in)))
				return -EFAULT;
			
			ret = evfs_dentry_read(sb, &k_args);
			if (copy_to_user(&(u_args->out), &(k_args.out),
						sizeof(k_args.out)))
				return -EFAULT;

			return ret;
		}
		case EXT4_EVFS_DEN_DELETE:
		{
			struct ext4_evfs_de_delete_args k_args;
			struct ext4_evfs_de_delete_args __user * u_args =
				(struct ext4_evfs_de_delete_args __user *) arg;
			if (copy_from_user(&(k_args.in), &(u_args->in), sizeof(k_args.in)))
				return -EFAULT;

			k_args.in.name[sizeof(k_args.in.name) - 1] = 0;
			return evfs_dentry_delete(sb, &k_args);
		}
		case EXT4_EVFS_DEN_UPDATE:
		{
			struct ext4_evfs_de_update_args k_args;
			struct ext4_evfs_de_update_args __user * u_args =
				(struct ext4_evfs_de_update_args __user *) arg;
			if (copy_from_user(&(k_args.in), &(u_args->in), sizeof(k_args.in)))
				return -EFAULT;

			ret = evfs_dentry_update(sb, &k_args);
			return ret;
		}
		case EXT4_EVFS_EXT_READ:
		{
			struct ext4_evfs_ext_read_args k_args;
			struct ext4_evfs_ext_read_args __user * u_args =
				(struct ext4_evfs_ext_read_args __user *) arg;
			if (copy_from_user(&(k_args.in), &(u_args->in), sizeof(k_args.in)))
				return -EFAULT;
			
			ret = evfs_extent_read(sb, &k_args);
			if (copy_to_user(&(u_args->out), &(k_args.out), sizeof(k_args.out)))
				return -EFAULT;

			size_t ext_arr_size = sizeof(struct ext4_evfs_ext) * k_args.out.num_exts;
			k_args.out.exts = (struct ext4_evfs_ext *) kmalloc(ext_arr_size, GFP_KERNEL);
			if (copy_to_user(u_args->out.exts, k_args.out.exts, ext_arr_size))
				return -EFAULT;

			return ret;
		}
		default:
			return -ENOTTY;
	}
}

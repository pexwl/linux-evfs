#include <linux/types.h>
#include <linux/fs.h>
#include <linux/jbd2.h>
#include <linux/quotaops.h>
#include "ioctl_evfs.h"
#include "ext4.h"
#include "ext4_jbd2.h"

// static helper function declaration
extern long evfs_ext4_hello(void);
extern long evfs_ext4_block_alloc(struct inode * ino, struct super_block * sb, ext4_fsblk_t bindex);
extern long evfs_ext4_inode_alloc(struct super_block * sb, ino_t iindex);


/**
 * @brief hello from evfs
 */
extern long evfs_ext4_hello() {
    printk(KERN_INFO "Hello, bad apple!\n");
    return 0;
}

// ext4_evfs_block_alloc
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
extern long evfs_ext4_block_alloc(struct inode * ino, struct super_block * sb, ext4_fsblk_t bindex) {
    // get the group number and block offset
    ext4_group_t group;
    ext4_grpblk_t offset;
    ext4_get_group_no_and_offset(sb, bindex, &group, &offset);
    int ret = 0, err = 0;

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

    ext4_lock_group(sb, group);
    if (ext4_test_and_set_bit(offset, bitmap_bh->b_data)) {
        ret = -EEXIST;
        ext4_unlock_group(sb, group);
        goto stop_journal;
    }
    ext4_unlock_group(sb, group);

    if ((err = ext4_handle_dirty_metadata(handle, NULL, bitmap_bh)) < 0) {
        ret = err;
        goto stop_journal;
    }

    if ((err = ext4_journal_get_write_access(handle, sb, grpdsc_bh, EXT4_JTR_NONE)) < 0) {
        ret = err;
        goto stop_journal;
    }

    ext4_lock_group(sb, group);
    // decrease free count
    u32 free_blocks_count = grpdsc->bg_free_blocks_count_hi << 16 | grpdsc->bg_free_blocks_count_lo;
    free_blocks_count -= 1;
    grpdsc->bg_free_blocks_count_hi = (u16) (free_blocks_count >> 16);
    grpdsc->bg_free_blocks_count_lo = (u16) (free_blocks_count & 0x0FFFF);

    // modify the checksum
    if (ext4_has_group_desc_csum(sb) && (grpdsc->bg_flags & cpu_to_le16(EXT4_BG_BLOCK_UNINIT))) {
		grpdsc->bg_flags &= cpu_to_le16(~EXT4_BG_BLOCK_UNINIT);
		ext4_free_group_clusters_set(sb, grpdsc,
			ext4_free_clusters_after_init(sb, group, grpdsc));
        ext4_block_bitmap_csum_set(sb, grpdsc, grpdsc_bh);
        ext4_group_desc_csum_set(sb, group, grpdsc);
    }
    ext4_unlock_group(sb, group);

    if ((err = ext4_handle_dirty_metadata(handle, ino, grpdsc_bh)) < 0) {
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

/** modify this for inode allocation
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
extern long evfs_ext4_inode_alloc(struct super_block * sb, ino_t iindex) {
    ext4_group_t group = iindex / EXT4_INODES_PER_GROUP(sb);
    ino_t offset = iindex % EXT4_INODES_PER_GROUP(sb);
    int ret = 0, err = 0;

    struct buffer_head * bitmap_bh = ext4_read_inode_bitmap(sb, group);
    if (IS_ERR(bitmap_bh)) {
        return PTR_ERR(bitmap_bh); // error from ext4_read_inode_bitmap
    }

    // read the group descriptor
    struct buffer_head * grpdsc_bh;
    struct ext4_group_desc * grpdsc = ext4_get_group_desc(sb, group, &grpdsc_bh);

    if (!grpdsc) {
        brelse(bitmap_bh);
        return (long) -EINVAL;
    }

    // create the inode
    struct inode * ino = new_inode(sb);
    if (!ino) {
        ret = -ENOMEM;
        goto out;
    }

    if ((err = dquot_initialize(ino))) {
        ret = err;
        goto out;
    }

    handle_t * handle = ext4_journal_start_with_reserve(ino, EXT4_HT_MAP_BLOCKS, 3, 0);
   
    if (IS_ERR(handle)) {
        ret = PTR_ERR(handle);
        goto out;
    }

    if ((err = ext4_journal_get_write_access(handle, sb, bitmap_bh, EXT4_JTR_NONE)) < 0) {
        ret = err;
        goto stop_journal;
    }
    
    ext4_lock_group(sb, group);
    if (ext4_test_and_set_bit(offset, bitmap_bh->b_data)) {
        ret = -EEXIST;
        ext4_unlock_group(sb, group);
        goto stop_journal;
    }
    ext4_unlock_group(sb, group);

    if ((err = ext4_handle_dirty_metadata(handle, NULL, bitmap_bh)) < 0) {
        ret = err;
        goto stop_journal;
    }

    struct ext4_inode_info *ei = EXT4_I(ino);

    ino->i_ino = iindex;
    ino->i_mode = S_IFREG | 0644; // use file for now, may change it later
    ino->i_blocks = 0; // optimal IO size (for stat), not the fs block size; refer to ialloc.c:1253
    simple_inode_init_ts(ino); // set time
    set_nlink(ino, 1); // make sure the inode is not garbage since we want to use it later
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

    // TODO: confirm with prof do we need to calculate
    // inode checksum here if inode has checksum enabled

    // you must locked a dangling inode here
    if (insert_inode_locked(ino) < 0) {
        ret = -EIO;
        goto stop_journal;
    }

    // allocate quota for inode
    if ((err = dquot_alloc_inode(ino))) {
        ret = err;
        goto fail_drop;
    }

    // note that ext4_mark_inode_dirty get write access to journal, 
    // mark journal dirty implicitly and stop the journal,
    // so we do NOT need them here
    if ((err = ext4_mark_inode_dirty(handle, ino))) {
        ret = err;
        goto fail_free_drop;
    }

    // modify the metadata in the block group descriptor
    if ((err = ext4_journal_get_write_access(handle, sb, grpdsc_bh, EXT4_JTR_NONE)) < 0) {
        ret = err;
        goto fail_free_drop;
    }

    // modify the free inode count
    ext4_lock_group(sb, group);
    u32 free_inodes_count = grpdsc->bg_free_inodes_count_hi << 16 | grpdsc->bg_free_inodes_count_lo;
    free_inodes_count -= 1;
    grpdsc->bg_free_inodes_count_hi = (u16) (free_inodes_count >> 16);
    grpdsc->bg_free_inodes_count_lo = (u16) (free_inodes_count & 0x0FFFF);

    // modify the checksum
    if (ext4_has_group_desc_csum(sb) && (grpdsc->bg_flags & cpu_to_le16(EXT4_BG_BLOCK_UNINIT))) {
		grpdsc->bg_flags &= cpu_to_le16(~EXT4_BG_BLOCK_UNINIT);
		ext4_free_group_clusters_set(sb, grpdsc,
			ext4_free_clusters_after_init(sb, group, grpdsc));
        ext4_block_bitmap_csum_set(sb, grpdsc, grpdsc_bh);
        ext4_group_desc_csum_set(sb, group, grpdsc);
    }
    ext4_unlock_group(sb, group);

    if ((err = ext4_handle_dirty_metadata(handle, ino, grpdsc_bh)) < 0) {
        ret = err;
        goto fail_free_drop;
    }

    goto stop_journal;

fail_free_drop:
    dquot_free_inode(ino);

fail_drop:
    clear_nlink(ino);
    unlock_new_inode(ino);
    iput(ino);

    // if anything fails during journal, let's stop journaling
stop_journal:
    if ((err = ext4_journal_stop(handle)) < 0) {
        ret = err;
        goto out;
    }

out:
    brelse(bitmap_bh);
    return (long) ret;
}

long ext4_evfs_entry(unsigned int cmd, struct inode * ino, struct super_block * sb, unsigned long arg) {
    switch(cmd) {
        case EXT4_IOC_EVFS_HELLO:
            return evfs_ext4_hello();
        case EXT4_IOC_EVFS_BLK_ALLOC:
            return evfs_ext4_block_alloc(ino, sb, (ext4_fsblk_t) arg);
        case EXT4_IOC_EVFS_INO_ALLOC:
            return evfs_ext4_inode_alloc(sb, (ino_t) arg);
        default:
            return -ENOTTY;
    }
}

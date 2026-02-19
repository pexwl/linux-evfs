#include <linux/types.h>
#include "ioctl_evfs.h"
#include "ext4.h"
#include "ext4_jbd2.h"

/**
 * @brief hello from evfs
 */
long evfs_hello() {
    printk(KERN_INFO "Hello, bad apple!\n");
    return 0;
}

/**
 * @brief journal a block allocation (?to an extent).
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
 *   * -ERRBLKSET: already allocated;
 *   * -ERRNOGRPD: fail to load group descriptor. either rcu deference fails
 *     or group index out of range;
 *   * -ERRNO: errors from ext4 functions;
 */
long evfs_journal_block_alloc(struct inode * ino, struct super_block * sb, ext4_fsblk_t bindex) {
    ext4_group_t blk_grp_i;
    ext4_grpblk_t blk_rel_i;
    // get the group number and block offset
    ext4_get_group_no_and_offset(sb, bindex, &blk_grp_i, &blk_rel_i);

    // block bitmap: turn the corresponding block bitmap to 1
    struct buffer_head * bh_bbmap = ext4_read_block_bitmap(sb, blk_grp_i);
    if (IS_ERR(bh_bbmap)) {
        brelse(bh_bbmap); // release the buffer head
        return PTR_ERR(bh_bbmap); // error from ext4_read_block_bitmap
    }

    // read the group descriptor
    struct buffer_head * bh_gdp;
    struct ext4_group_desc * gdp = ext4_get_group_desc(sb, blk_grp_i, &bh_gdp);

    if (!gdp) {
        brelse(bh_bbmap);
        return -ERRNOGRPD;
    }

    /**
     * create journal handle >>> For now, 2 blocks and 0 rsv_blocks
     * @brief journal_start(struct inode * ino, int type, int blocks, int rsv_blocks)
     * @param inode[struct inode *]
     * @param type[int]: debugging info for journal operation (here, it's mapping logical
     * block to physical block)
     * @param blocks[int]: number of data change (how many operations that touch bitmap/
     * block/group/cluster/superblock)
     * @param rsv_blocks[int]: extra data change you might touch (worse case: e.g. when you
     * need to split a extent, create a block to hold the extent)
     */
    handle_t * handle = ext4_journal_start_with_reserve(ino, EXT4_HT_MAP_BLOCKS, 2, 0);
    int err = 0, tmperr;

    // modify the block bitmap
    if ((tmperr = ext4_journal_get_write_access(handle, sb, bh_bbmap, EXT4_JTR_NONE)) < 0) {
        err = tmperr;
        goto __evfs_journal_data_block_alloc_out;
    }

    if (ext4_test_and_set_bit(bindex, &(bh_bbmap->b_state))) {
        err = -ERRBLKSET;
        goto __evfs_journal_data_block_alloc_out;
    }
    ext4_block_bitmap_csum_set(sb, gdp, bh_bbmap);

    if ((tmperr = ext4_handle_dirty_metadata(handle, ino, bh_bbmap)) < 0) {
        err = tmperr;
        goto __evfs_journal_data_block_alloc_out;
    }

    // modify the metadata in the block group descriptor
    if ((tmperr = ext4_journal_get_write_access(handle, sb, bh_gdp, EXT4_JTR_NONE)) < 0) {
        err = tmperr;
        goto __evfs_journal_data_block_alloc_out;
    }

    // modify the free block count
    u32 free_blocks_count = gdp->bg_free_blocks_count_hi << 16 | gdp->bg_free_blocks_count_lo;
    free_blocks_count -= 1;
    gdp->bg_free_blocks_count_hi = (u16) (free_blocks_count >> 16);
    gdp->bg_free_blocks_count_lo = (u16) (free_blocks_count & ~(0xFFFF << 16));

    // modify the checksum
    ext4_group_desc_csum_set(sb, blk_grp_i, gdp);

    if ((tmperr = ext4_handle_dirty_metadata(handle, ino, bh_gdp)) < 0) {
        err = tmperr;
        goto __evfs_journal_data_block_alloc_out;
    }

    if ((tmperr = ext4_journal_stop(handle)) < 0) {
        err = tmperr;
        goto __evfs_journal_data_block_alloc_out;
    }

__evfs_journal_data_block_alloc_out:
    brelse(bh_bbmap);
    brelse(bh_gdp);
    return (long) err;
}

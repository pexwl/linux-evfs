#include <linux/fs.h>
#include <linux/f2fs_fs.h>
#include <linux/evfs.h>
#include <linux/math.h>
#include "f2fs.h"
#include "segment.h"
#include "evfs.h"

/* there is a similar helper available in kernel 6.9;
 * In 6.8, the arithematic is in fs/f2fs/f2fs.h: `inc_valid_block_count`
 */
static unsigned int f2fs_evfs_get_available_block_count(struct f2fs_sb_info * sbi)
{
	// main area block counts - overprovisioning
	unsigned int avail_user_block_count = sbi->user_block_count - sbi->current_reserved_blocks;
	// skip checking allow_reserved_blocks since only inode can has root_reserved_blocks
	if (F2FS_IO_ALIGNED(sbi)) avail_user_block_count -=
		sbi->blocks_per_seg * SM_I(sbi)->additional_reserved_segments;

	if (unlikely(is_sbi_flag_set(sbi, SBI_CP_DISABLED))) {
		if (avail_user_block_count > sbi->unusable_block_count) {
			avail_user_block_count -= sbi->unusable_block_count;
		} else {
			avail_user_block_count = 0;
		}
	}

	return avail_user_block_count;
}

/* get the maximum free sections without triggering fg gc
 */
static unsigned int f2fs_evfs_get_max_free_secs(struct f2fs_sb_info * sbi)
{
	unsigned int lower_secs, upper_secs;
	bool curseg_space;

	__get_secs_required(sbi, &lower_secs, &upper_secs, &curseg_space);

	unsigned int free_secs = free_sections(sbi);	
	unsigned int threshold = reserved_sections(sbi) + F2FS_EVFS_SECS_MARGIN;
	if (curseg_space) {
		threshold += lower_secs;
	} else {
		threshold += upper_secs;
	}

	if (free_secs > threshold) return free_secs - threshold - 1;
	return 0;
}

long f2fs_evfs_hello(struct super_block *);
long f2fs_evfs_extent_alloc(struct super_block *, struct f2fs_evfs_ext_alloc_args *);

/**
 * @brief hello from ext4_evfs
 */
long f2fs_evfs_hello(struct super_block * sb)
{
    (void) sb;
	printk(KERN_INFO "Hello, bad apple!\n");
	return 0;
}

long f2fs_evfs_extent_alloc(struct super_block * sb, struct f2fs_evfs_ext_alloc_args * args)
{
	long ret = 0;
	struct f2fs_sb_info * sbi = sb->s_fs_info;
	struct sit_info * sit_i = SIT_I(sbi);
	struct f2fs_sm_info * segman = SM_I(sbi);
	struct free_segmap_info * fsm = segman->free_info;
	unsigned int start = args->in.start;
	unsigned int len = args->in.len;
	unsigned int max_len = MAX_BLKADDR(sbi) - start;
	unsigned int diff = 0;

	// narrow the range if start is at reserved zone and len exceeds maximum available blocks
	if (start < segman->main_blkaddr) {
        diff = segman->main_blkaddr - start;
        if (diff >= len) {
            len = 0;
        } else {
            len -= diff; 
        }

        diff = 0;
        start = segman->main_blkaddr;
    }

	if (len > max_len) len = max_len;
	if (len == 0) goto nospace;

    unsigned int start_rel = start - segman->main_blkaddr;
    unsigned int blocks_per_group = blocks_per_seg;
	unsigned int s_start = start_rel / sbi->blocks_per_group;
	unsigned int s_end = (start_rel + len) / sbi->blocks_per_group;

	// unsigned int blk_offset = start % sbi->blocks_per_seg;
	// DONE: down_read ckpt lock, down_read curseg_lock and down_write sentry_lock
	f2fs_lock_op(sbi);
	f2fs_down_read(&(segman->curseg_lock));
	down_write(&(sit_i->sentry_lock));

	unsigned int free_secs = 0;
	spin_lock(&(fsm->segmap_lock));
	free_secs = f2fs_evfs_get_max_free_secs(sbi);

	// TODO: find maximum segments we can allocate
    unsigned int max_s_start = 0, max_s_len = 0;
    unsigned int arr[s_end - s_start + 1], alen = s_end - s_start, last_used;
    for (unsigned int i = 0, ri = alen; i <= alen; i++, ri--) {
        if (test_bit(g_start + ri, fsm->free_segmap)) {
            last_used = ri;
            arr[ri] = 0;
            continue;
        }

        arr[ri] = last_used - ri;
        if (arr[ri] > free_secs) arr[ri] = free_secs;
    }

    for (unsigned int i = 0; i < alen; i++) {
        if (arr[i] > max_g_len) {
            max_g_len = arr[i];
            max_g_start = i + g_start;    
        }
    }

	spin_unlock(&(fsm->segmap_lock));

	up_write(&(sit_i->sentry_lock));
	// TODO: update SSA here
	f2fs_up_read(&(segman->curseg_lock));
	f2fs_unlock_op(sbi);

	// NOOP: drop per-cpu alloc_valid_block_count and total_valid_block_count after allocation

nospace:
	ret = -ENOSPC;
	args->out.start = 0;
	args->out.len = 0;

terminate:
	return ret;
}

long f2fs_ioctl_evfs(unsigned int cmd, struct file * filp, unsigned long arg)
{
	long ret = 0;
	struct inode * inode = file_inode(filp);
	struct super_block * sb = inode->i_sb;
	switch(cmd) {
        case F2FS_EVFS_HELLO:
            return f2fs_evfs_hello(sb);
		case F2FS_EVFS_EXT_ALLOC:
			struct f2fs_evfs_ext_alloc_args k_args;
			struct f2fs_evfs_ext_alloc_args __user * u_args =
				(struct f2fs_evfs_ext_alloc_args __user *) arg;

			if (copy_from_user(&(k_args.in), &(u_args->in),
						sizeof(k_args.in)))
				return -EFAULT;
			
			ret = f2fs_evfs_extent_alloc(sb, &k_args);
			if (copy_to_user(&(u_args->out), &(k_args.out),
						sizeof(k_args.out)))
				return -EFAULT;

			return ret;
		default:
			return -ENOTTY;
	}
}


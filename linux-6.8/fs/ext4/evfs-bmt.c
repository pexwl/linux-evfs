#include <linux/types.h>
#include <linux/slab.h>
#include "evfs-bmt.h"

static int evfs_bm_alloc(struct evfs_bm_tracker * tracker, size_t bitmap_num)
{
	struct evfs_bitmap * b;
	b = (struct evfs_bitmap *) kzalloc(
		sizeof(struct evfs_bitmap), GFP_KERNEL);

	if (b == NULL) return -ENOMEM;
	tracker->bitmaps[bitmap_num] = b;
	return 0;
}

static void evfs_bm_free(struct evfs_bm_tracker * tracker, size_t bitmap_num)
{
	kfree(tracker->bitmaps[bitmap_num]);
	tracker->bitmaps[bitmap_num] = NULL;
}

static bool evfs_bm_test(struct evfs_bm_tracker * tracker, size_t bitmap_num)
{
	return tracker->bitmaps[bitmap_num] != NULL;
}

int evfs_bm_tracker_init(struct evfs_bm_tracker ** tracker, size_t num_bits)
{
	struct evfs_bm_tracker * t;
	t = (struct evfs_bm_tracker *) kmalloc(
		sizeof(struct evfs_bm_tracker), GFP_KERNEL);

	if (t == NULL) return -ENOMEM;
	size_t bits_per_bm = EXT4_EVFS_GRP_BM_LEN * sizeof(size_t) * 8;
	size_t num_bitmaps = (num_bits + bits_per_bm - 1) / bits_per_bm; // ceil(num_bitmaps/bits_per_bm)
	t->bitmaps = (struct evfs_bitmap **) kzalloc(
			sizeof(struct evfs_bitmap *) * num_bitmaps, GFP_KERNEL);

	if (t->bitmaps == NULL) return -ENOMEM;
	t->rwsems = (struct rw_semaphore *) kmalloc(
		sizeof(struct rw_semaphore) * num_bitmaps, GFP_KERNEL);

	if (t->rwsems == NULL) return -ENOMEM;
	for (size_t i = 0; i < num_bitmaps; i++) {
		init_rwsem(&(t->rwsems[i]));
	}

	t->num_bitmaps = num_bitmaps;
	*tracker = t;
	return 0;
}

int evfs_bm_tracker_final(struct evfs_bm_tracker * tracker)
{
	if (tracker == NULL) return -EFAULT;
	for (size_t i = 0; i < tracker->num_bitmaps; i++) {
		down_write(&(tracker->rwsems[i])); // lock as a writer
		if (evfs_bm_test(tracker, i)) evfs_bm_free(tracker, i);
		up_write(&(tracker->rwsems[i]));
	}

	kfree(tracker->bitmaps);
	kfree(tracker->rwsems);
	kfree(tracker);
	return 0;
}

int evfs_bit_wrapper(struct evfs_bm_tracker * tracker, evfs_bitop * ebitop, unsigned long nr)
{
	if (tracker == NULL) return -EFAULT;
	size_t bits_per_bm = EXT4_EVFS_GRP_BM_LEN * sizeof(size_t) * 8;
	size_t bitmap_num = nr / bits_per_bm;
	size_t bit_offset = nr % bits_per_bm;

	if (bitmap_num >= tracker->num_bitmaps) return -EINVAL;
	down_read(&(tracker->rwsems[bitmap_num]));
	if (!evfs_bm_test(tracker, bitmap_num)) {
		up_read(&(tracker->rwsems[bitmap_num]));
		down_write(&(tracker->rwsems[bitmap_num]));
		// re-check allocation to prevent another thread allocating the bitmap
		if (!evfs_bm_test(tracker, bitmap_num)) evfs_bm_alloc(tracker, bitmap_num);
		downgrade_write(&(tracker->rwsems[bitmap_num])); // downgrade to a reader
	}

	int ret = 0;
	if (ebitop->type == EBO_VOID) {
		ebitop->op_v(bit_offset, tracker->bitmaps[bitmap_num]->data);
	} else {
		ret = (int) ebitop->op_b(bit_offset, tracker->bitmaps[bitmap_num]->data);	
	}

	up_read(&(tracker->rwsems[bitmap_num]));
	return ret;
}


#include <linux/rwsem.h>

#ifndef _EXT4_EVFS_BMT
#define _EXT4_EVFS_BMT

#define EXT4_EVFS_GRP_BM_LEN 1 // 64b on 64-bit or 32b on 32-bit

typedef void (* evfs_bitop_v) (long, volatile unsigned long *);
typedef bool (* evfs_bitop_b) (long, volatile unsigned long *);

typedef enum evfs_bitop_type
{
	EBO_VOID,
	EBO_BOOL
} evfs_bitop_type;

typedef struct evfs_bitop
{
	evfs_bitop_type type;
	union {
		evfs_bitop_v op_v;
		evfs_bitop_b op_b;
	};
} evfs_bitop;

struct evfs_bitmap
{
	unsigned long data[EXT4_EVFS_GRP_BM_LEN];
};

struct evfs_bm_tracker
{
	struct evfs_bitmap ** bitmaps;
	struct rw_semaphore * rwsems;
	size_t num_bitmaps;
};

int evfs_bm_tracker_init(struct evfs_bm_tracker **, size_t);
int evfs_bm_tracker_final(struct evfs_bm_tracker *);
int evfs_bit_wrapper(struct evfs_bm_tracker *, evfs_bitop *, unsigned long);

#endif


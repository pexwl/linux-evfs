void bruh(){
	percpu_counter_add(&(sbi->alloc_valid_block_count), len);

	// pre-allocate total_valid_block_count to prevent other from reserving more
	spin_lock(&(sbi->stat_lock));	
	unsigned int num_avail_blks = f2fs_evfs_get_available_block_count(sbi);
	if (num_avail_blks > len) {
		len = num_avail_blks;
		diff = num_avail_blks - len;
	}

	sbi->total_valid_block_count += len;
	spin_unlock(&(sbi->stat_lock));

	if (diff > 0) {
		percpu_counter_sub(&(sbi->alloc_valid_block_count), diff);
		diff = 0;
	}
}

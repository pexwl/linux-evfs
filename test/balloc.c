#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/evfs.h>

int main(int argc, char * argv[]) {
	if (argc != 3) {
		fprintf(stderr, "usage: balloc block_number file\n");
		return 1;
	}

	char *endptr;
	unsigned long blk_num = strtoul(argv[1], &endptr, 10);

	if (endptr == argv[1] || blk_num < 0) {
		fprintf(stderr, "no valid block number found %lu \n", blk_num);
		return -1;
	}

	char filename[64];
	strncpy(filename, argv[2], 64);

	int fd = open(filename, O_RDONLY);
	return (int) ioctl(fd, EXT4_IOC_EVFS_BLK_ALLOC, blk_num); // allocate a block
}

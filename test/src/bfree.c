#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/evfs.h>
#include "helper.h"

int main(int argc, char * argv[]) {
	if (usage(argc, 3, "bfree block_number img"))
		return 1;

	
	unsigned int blk_num;
	if (str2ul(argv[1], &blk_num, "block_number")) return 1;

	char filename[_TEVFS_EXT4_PATHLEN];
	null_terminated_strncpy(filename, argv[2], _TEVFS_EXT4_PATHLEN);

	int fd = open(filename, O_RDONLY);
	if (fd < 0) {
		perror("open");
		return 1;
	}

	if (ioctl(fd, EXT4_EVFS_BLK_FREE, (unsigned long) blk_num) < 0) { // allocate a block
		perror("ioctl ALLOCATE_FREE");
		close(fd);
		return 1;
	}

	close(fd);
	return 0;
}

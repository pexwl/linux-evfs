#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include "evfs_cmd.h"

int main(int argc, char * argv[]) {
	if (argc != 2) {
		fprintf(stderr, "usage: test-balloc file\n");
		return 1;
	}

	char filename[64];
	strncpy(filename, argv[1], 64);

	int fd = open(filename, O_RDONLY);
	int ret = ioctl(fd, EXT4_IOC_EVFS_BLK_ALLOC, 2048); // allocating inode 100

	return ret;
}

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include "evfs_cmd.h"

int main(int argc, char * argv[]) {
	if (argc != 2) {
		fprintf(stderr, "usage: test-hello file\n");
		return 1;
	}

	char pathname[64];
	strncpy(pathname, argv[1], 64);

	int fd = open(pathname, O_RDONLY);
	ioctl(fd, EXT4_IOC_EVFS_HELLO);

	return 0;
}

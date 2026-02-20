#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include "evfs_cmd.h"

int main() {

	FILE *test;
	test = fopen("/tmp/ext4-a0/abc.txt", "r");
	int fd = fileno(test);
	ioctl(fd, EXT4_IOC_EVFS_HELLO);

	return 0;
}

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/evfs.h>
#include "helper.h"

int main(int argc, char * argv[]) {
	if (usage(argc, 2, "hello img"))
		return 1;

	char pathname[_TEVFS_EXT4_PATHLEN];
	null_terminated_strncpy(pathname, argv[1], _TEVFS_EXT4_PATHLEN);

	int fd = open(pathname, O_RDONLY);
	if (fd < 0) {
		perror("open");
		return 1;
	}

	int ret = 0;
	if (ioctl(fd, EXT4_EVFS_HELLO) < 0) {
		perror("ioctl HELLO");
		ret = 1;
	}
	close(fd);

	return ret;
}

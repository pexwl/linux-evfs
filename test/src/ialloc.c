#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/evfs.h>
#include "helper.h"

int main(int argc, char * argv[]) {
	if (usage(argc, 3, "ialloc inode_number img"))
		return 1;

	unsigned int ino_num = 0;
	if (str2ul(argv[1], &ino_num, "inode number") || ino_num < 1)
		return 1;

	char filename[_TEVFS_EXT4_PATHLEN];
	null_terminated_strncpy(filename, argv[2], _TEVFS_EXT4_PATHLEN);

	int fd = open(filename, O_RDONLY);
	if (fd < 0) {
		perror("open");
		return 1;
	}

	if (ioctl(fd, EXT4_EVFS_INO_ALLOC, (unsigned long) ino_num) < 0) { // allocate inode
		perror("ioctl ALLOCATE_INODE");
		return 1;
	}
	close(fd);

	return 0;
}

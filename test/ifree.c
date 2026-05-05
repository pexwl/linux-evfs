#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/evfs.h>

int main(int argc, char * argv[]) {
	if (argc != 3) {
		fprintf(stderr, "usage: ifree inode_number file\n");
		return 1;
	}

	char *endptr;
	unsigned long ino_num = strtoul(argv[1], &endptr, 10);

	if (endptr == argv[1] || ino_num < 1) {
		fprintf(stderr, "no valid inode number found %lu \n", ino_num);
		return -1;
	}

	char filename[64];
	strncpy(filename, argv[2], 64);

	int fd = open(filename, O_RDONLY);
	return (int) ioctl(fd, EXT4_IOC_EVFS_INO_FREE, ino_num); // free inode
}

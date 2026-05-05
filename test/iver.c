#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/evfs.h>

int main(int argc, char * argv[]) {
	if (argc != 2) {
		fprintf(stderr, "usage: iver file\n");
		return 1;
	}

	char pathname[64];
	strncpy(pathname, argv[1], 64);

	int ret = 0, err = 0;
	int fd = open(pathname, O_RDONLY);
	unsigned long long i_version = 0;
	if ((err = ioctl(fd, EXT4_IOC_EVFS_IVER, (unsigned long) &i_version)) < 0) {
		ret = err;
	}
	printf("%llu\n", i_version);

	return err;
}

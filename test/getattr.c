#include <stdio.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/evfs.h>

void get_time(time_t time, size_t fmt_time_len, char * fmt_time) {
	struct tm * time_info = localtime(&time);
	strftime(fmt_time, fmt_time_len, "%Y-%m-%d %H:%M:%S", time_info);
}

int main(int argc, char * argv[]) {
	if (argc != 2) {
		fprintf(stderr, "usage: getattr file\n");
		return 1;
	}

	char pathname[64];
	strncpy(pathname, argv[1], 64);

	int ret = 0, err = 0;
	int fd = open(pathname, O_RDONLY);
	struct evfs_stat stat;
	if ((err = (int) ioctl(fd, EXT4_IOC_EVFS_GETATTR, (unsigned long) &stat)) < 0) {
		ret = err;
	}

	size_t tlen = 64;
	char atime[tlen];
	char mtime[tlen];
	char ctime[tlen];

	get_time((time_t) stat.est_atime, tlen, atime);
	get_time((time_t) stat.est_mtime, tlen, mtime);
	get_time((time_t) stat.est_ctime, tlen, ctime);

	printf("Device #: %lu\n", stat.est_dev);	
	printf("Inode #: %lu\n", stat.est_ino);
	printf("Mode: %o\n", stat.est_mode);
	printf("Link count: %u\n", stat.est_nlink);
	printf("User ID: %u\n", stat.est_uid);
	printf("Group ID: %u\n", stat.est_gid);
	printf("Root Device #: %lu\n", stat.est_rdev);
	printf("Version: %lu\n", stat.est_version);
	printf("Size: %l\n", stat.est_size);
	printf("Block Size: %d\n", stat.est_blksize);
	printf("Accessed: %s\n", atime);
	printf("Modified: %s\n", mtime);
	printf("Created: %s\n", ctime);

	return ret;
}

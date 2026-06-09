#include <stdio.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/evfs.h>
#include "helper.h"

void get_time(time_t time, size_t fmt_time_len, char * fmt_time) {
	struct tm * time_info = localtime(&time);
	strftime(fmt_time, fmt_time_len, "%Y-%m-%d %H:%M:%S", time_info);
}

int main(int argc, char * argv[]) {
	if (usage(argc, 3, "iread ino_num img"))
		return 1;

	unsigned long iindex;
	if (str2ul(argv[1], &iindex, "inode number")) {
		return 1;
	}

	char pathname[_TEVFS_EXT4_PATHLEN];
	null_terminated_strncpy(pathname, argv[2], _TEVFS_EXT4_PATHLEN);

	int fd = open(pathname, O_RDONLY);
	if (fd < 0) {
		perror("open");
		return 1;
	}

	struct evfs_ino_read_args args;
	args.in.iindex = iindex;
	evfs_stat * stat = &(args.out);
	if (ioctl(fd, EXT4_EVFS_INO_READ, (unsigned long) &args) < 0) {
		perror("ioctl STAT");
		close(fd);
		return 1;
	}
	close(fd);

	size_t tlen = 64;
	char atime[tlen];
	char mtime[tlen];
	char ctime[tlen];

	get_time((time_t) stat->est_atime, tlen, atime);
	get_time((time_t) stat->est_mtime, tlen, mtime);
	get_time((time_t) stat->est_ctime, tlen, ctime);

	printf("File: %s\n", pathname);
	printf("Size: %ld\tBlocks: %d\tMode: %o\tUID, GID: %u, %u\n",
		stat->est_size, stat->est_blksize, stat->est_mode, stat->est_uid, stat->est_gid);
	printf("Dev, Root Dev: %lu, %lu\tInode #: %lu\tLinks: %u\n",
		stat->est_dev, stat->est_rdev, stat->est_ino, stat->est_nlink);	
	printf("Version: %lu\n", stat->est_version);
	printf("Accessed: %s.%lu\n", atime, stat->est_atime_nsec);
	printf("Modified: %s.%lu\n", mtime, stat->est_mtime_nsec);
	printf("Created: %s.%lu\n", ctime, stat->est_ctime_nsec);

	return 0;
}

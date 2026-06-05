#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/ext4.h>
#include "helper.h"

int main(int argc, char * argv[]) {
	if (usage(argc, 5, "extent_swap start len forig fdonr"))
		return 1;

	struct move_extent me;
	memset(&me, 0, sizeof(struct move_extent));

	if (str2ull(argv[1], &(me.orig_start), "start block")) return 1;
	me.donor_start = me.orig_start;

	if (str2ull(argv[2], &(me.len), "block length")) return 1;

	char orig[_TEVFS_EXT4_PATHLEN];
	null_terminated_strncpy(orig, argv[3], _TEVFS_EXT4_PATHLEN);
	char donr[_TEVFS_EXT4_PATHLEN];
	null_terminated_strncpy(donr, argv[4], _TEVFS_EXT4_PATHLEN);

	int fd_orig = open(orig, O_RDWR);
	if (fd_orig < 0) {
		perror("open >> original file");
		return 1;
	}

	int fd_donr = open(donr, O_RDWR);
	if (fd_donr < 0) {
		perror("open >> donor file");
		return 1;
	}

	me.donor_fd = fd_donr;

	fsync(fd_orig);
	fsync(fd_donr);

	// call ioctl
	if (ioctl(fd_orig, EXT4_IOC_MOVE_EXT, &me) < 0) {
		perror("ioctl MOVE_EXT");
		close(fd_orig);
		close(fd_donr);
		return 1;
	}

	printf("EXTENT SWAPPED >> MOVED: %llu\n", me.moved_len);

	int ret1 = posix_fadvise(fd_orig, 0, 0, POSIX_FADV_DONTNEED);
	int ret2 = posix_fadvise(fd_donr, 0, 0, POSIX_FADV_DONTNEED);

	printf("posix_fadvise fd_orig returned %d\n", ret1);
	printf("posix_fadvise fd_donr returned %d\n", ret2);

	close(fd_orig);
	close(fd_donr);
	return 0;
}

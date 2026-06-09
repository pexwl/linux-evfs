#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/ext4.h>
#include "helper.h"

int main(int argc, char * argv[]) {
	if (usage(argc, 5, "extent_move.native start len forig fdonr"))
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

	// call ioctl
	if (ioctl(fd_orig, EXT4_IOC_MOVE_EXT, &me) < 0) {
		perror("ioctl MOVE_EXT");
		close(fd_orig);
		close(fd_donr);
		return 1;
	}

	printf("EXTENT SWAPPED >> MOVED: %llu\n", me.moved_len);

	close(fd_orig);
	close(fd_donr);
	return 0;
}

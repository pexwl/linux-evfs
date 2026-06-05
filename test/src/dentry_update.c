#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/evfs.h>
#include "helper.h"

int main(int argc, char * argv[]) {
	if (usage(argc, 5, "dentry_update directory_inode_num dentry_index new_inode_num img"))
		return 1;

	struct ext4_evfs_de_update_args args;
	memset(&args, 0, sizeof(args));

	if (str2ul(argv[1], &(args.in.dir_ino_num), "dir_ino_num")) return 1;
	if (str2u(argv[2], &(args.in.target_dentry_index), "target_dentry_index")) return 1;
	if (str2ul(argv[3], &(args.in.new_ino_num), "new_ino_num")) return 1;

	char pathname[_TEVFS_EXT4_PATHLEN];
	null_terminated_strncpy(pathname, argv[4], _TEVFS_EXT4_PATHLEN);
	int fd = open(pathname, O_RDONLY);
	if (fd < 0) {
		perror("open");
		return 1;
	}

	if (ioctl(fd, EXT4_EVFS_DEN_UPDATE, &args) < 0) {
		perror("ioctl update_DENTRY");
		close(fd);
		return 1;
	}

	printf("updated directory inode %lu dentry index %u to inode num = %lu\n",
		args.in.dir_ino_num, args.in.target_dentry_index,
		args.in.new_ino_num);

	close(fd);
	return 0;
}

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/evfs.h>
#include "helper.h"

int main(int argc, char * argv[]) {
	if (usage(argc, 5, "dentry_add fname parent_inode_num child_inode_num img"))
		return 1;

	struct ext4_evfs_de_add_args args;
	null_terminated_strncpy(args.in.name, argv[1], _TEVFS_EXT4_PATHLEN);

	if (str2ul(argv[2], &(args.in.parent_ino_num), "parent inode number")) return 1;
	if (str2ul(argv[3], &(args.in.child_ino_num), "child inode number")) return 1;

	char pathname[_TEVFS_EXT4_PATHLEN];
	null_terminated_strncpy(pathname, argv[4], _TEVFS_EXT4_PATHLEN);

	int fd = open(pathname, O_RDONLY);
	if (fd < 0) {
		perror("open");
		return 1;
	}

	// call ioctl
	if (ioctl(fd, EXT4_EVFS_DEN_ADD, &args) < 0) {
		perror("ioctl ADD_DENTRY");
		close(fd);
		return 1;
	}

	printf("Successfully added directory entry:\n");
	printf("  Parent inode:	%llu\n", (unsigned long long)args.in.parent_ino_num);
	printf("  Child inode:	%llu\n", (unsigned long long)args.in.child_ino_num);
	printf("  Name:		'%s'\n", args.in.name);

	close(fd);
	return 0;
}

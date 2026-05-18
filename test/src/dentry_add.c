#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/evfs.h>
#include "helper.h"

int main(int argc, char * argv[]) {
	if (usage(argc, 5, "dentry_add fname parent_inode_num child_inode_num img"))
		return 1;

	struct ext4_evfs_de_add_args add_info;
	null_terminated_strncpy(add_info.name, argv[1], _TEVFS_EXT4_PATHLEN);

	if (str2ull(argv[2], &(add_info.parent_inode_number), "parent inode number")) return 1;
	if (str2ull(argv[3], &(add_info.child_inode_number), "child inode number")) return 1;

	add_info.file_type = 1; // #define EXT4_FT_REG_FILE 1

	char pathname[_TEVFS_EXT4_PATHLEN];
	null_terminated_strncpy(pathname, argv[4], _TEVFS_EXT4_PATHLEN);

	int fd = open(pathname, O_RDONLY);
	if (fd < 0) {
		perror("open");
		return 1;
	}

	// call ioctl
	if (ioctl(fd, EXT4_EVFS_DEN_ADD, &add_info) < 0) {
		perror("ioctl ADD_DENTRY");
		close(fd);
		return 1;
	}

	printf("Successfully added directory entry:\n");
	printf("  Parent inode:	%llu\n", (unsigned long long)add_info.parent_inode_number);
	printf("  Child inode:	%llu\n", (unsigned long long)add_info.child_inode_number);
	printf("  Name:		'%s'\n", add_info.name);

	close(fd);
	return 0;
}

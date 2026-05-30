#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/evfs.h>
#include "helper.h"

int main(int argc, char * argv[]) {
	if (usage(argc, 4, "dentry_delete directory_inode_num fname img"))
		return 1;

	struct ext4_evfs_de_delete_args args;
	memset(&args, 0, sizeof(args));

	if (str2ul(argv[1], &(args.in.dir_ino_num), "directory inode number"))
		return 1;
	
	char pathname[_TEVFS_EXT4_PATHLEN];
	null_terminated_strncpy(args.in.name, argv[2], _TEVFS_EXT4_PATHLEN);
	null_terminated_strncpy(pathname, argv[3], _TEVFS_EXT4_PATHLEN);

	int fd = open(pathname, O_RDONLY);
	if (fd < 0) {
		perror("open");
		return 1;
	}

	if (ioctl(fd, EXT4_EVFS_DEN_DELETE, &args) < 0) {
		perror("ioctl DELETE_DENTRY");
		close(fd);
		return 1;
	}

	printf("Deleted dentry '%s' from directory inode %lu\n",
		args.in.name, args.in.dir_ino_num);

	close(fd);
	return 0;
}

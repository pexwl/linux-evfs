#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/evfs.h>
#include "helper.h"

int main(int argc, char * argv[]) {
	if (usage(argc, 4, "dentry_delete directory_inode_num fname img"))
		return 1;

	struct ext4_evfs_de_delete_args delete_info;
	memset(&delete_info, 0, sizeof(delete_info));

	if (str2ull(argv[1], &(delete_info.dir_inode_number), "directory inode number"))
		return 1;
	
	char pathname[_TEVFS_EXT4_PATHLEN];
	null_terminated_strncpy(pathname, argv[2], _TEVFS_EXT4_PATHLEN);
	null_terminated_strncpy(delete_info.name, argv[3], _TEVFS_EXT4_PATHLEN);

	int fd = open(pathname, O_RDONLY);
	if (fd < 0) {
		perror("open");
		return 1;
	}

	if (ioctl(fd, EXT4_EVFS_DEN_DELETE, &delete_info) < 0) {
		perror("ioctl DELETE_DENTRY");
		close(fd);
		return 1;
	}

	printf("Deleted dentry '%s' from directory inode %lu\n",
		delete_info.name, delete_info.dir_inode_number);

	close(fd);
	return 0;
}

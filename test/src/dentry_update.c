#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/evfs.h>
#include "helper.h"

int main(int argc, char * argv[]) {
	if (usage(argc, 5, "dentry_update directory_inode_num dentry_index new_inode_num img"))
		return 1;

	struct ext4_evfs_de_update_args update_info;
	memset(&update_info, 0, sizeof(update_info));

	if (str2ull(argv[1], &(update_info.dir_inode_number), "dir_inode_number")) return 1;
	if (str2ul(argv[2], &(update_info.target_dentry_index), "target_dentry_index")) return 1;
	if (str2ul(argv[3], &(update_info.new_inode_number), "new_inode_number")) return 1;

	char pathname[_TEVFS_EXT4_PATHLEN];
	null_terminated_strncpy(pathname, argv[4], _TEVFS_EXT4_PATHLEN);
	int fd = open("/home/evie/code/evfs-sandbox/fileA", O_RDONLY);
	if (fd < 0) {
		perror("open");
		return 1;
	}

	if (ioctl(fd, EXT4_EVFS_DEN_UPDATE, &update_info) < 0) {
		perror("ioctl update_DENTRY");
		close(fd);
		return 1;
	}

	printf("updated directory inode %lu dentry index %u to inode num = %u\n",
		update_info.dir_inode_number, update_info.target_dentry_index,
		update_info.new_inode_number);

	close(fd);
	return 0;
}

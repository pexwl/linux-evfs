#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/evfs.h>
#include "helper.h"

int main(int argc, char * argv[]) {
	if (usage(argc, 4, "dentry_read dir_inode_number target_dentry_index img"))
		return 1;

	struct ext4_evfs_de_read_args read_info;
	memset(&read_info, 0, sizeof(read_info));

	read_info.dir_inode_number = atoi(argv[1]);
	read_info.target_dentry_index = atoi(argv[2]);

	char pathname[_TEVFS_EXT4_PATHLEN];
	null_terminated_strncpy(pathname, argv[3], _TEVFS_EXT4_PATHLEN);

	int fd = open(pathname, O_RDONLY);
	if (fd < 0) {
		perror("open");
		return 1;
	}

	if (ioctl(fd, EXT4_EVFS_DEN_READ, &read_info) < 0) {
		perror("ioctl READ_DENTRY");
		close(fd);
		return 1;
	}

	printf("Reading inode %lu directory entry %u\n", 
		read_info.dir_inode_number, read_info.target_dentry_index);
	printf("  Inode:	 %u\n", read_info.inode_number);
	printf("  Name:	  '%s'\n", read_info.name);
	printf("  Type:	  %u\n", read_info.file_type);
	printf("  Name len:  %u\n", read_info.name_len);
	return 0;
}

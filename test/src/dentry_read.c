#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/evfs.h>
#include "helper.h"

int main(int argc, char * argv[]) {
	if (usage(argc, 4, "dentry_read dir_ino_num target_dentry_index img"))
		return 1;

	struct ext4_evfs_de_read_args args;
	memset(&(args.out), 0, sizeof(args.out));

	args.in.dir_ino_num = atoi(argv[1]);
	args.in.target_dentry_index = atoi(argv[2]);

	char pathname[_TEVFS_EXT4_PATHLEN];
	null_terminated_strncpy(pathname, argv[3], _TEVFS_EXT4_PATHLEN);

	int fd = open(pathname, O_RDONLY);
	if (fd < 0) {
		perror("open");
		return 1;
	}

	if (ioctl(fd, EXT4_EVFS_DEN_READ, &args) < 0) {
		perror("ioctl READ_DENTRY");
		close(fd);
		return 1;
	}

	printf("Reading inode %lu directory entry %u\n", 
		args.in.dir_ino_num, args.in.target_dentry_index);
	printf("  Inode:	%lu\n", args.out.ino_num);
	printf("  Name: 	'%s'\n", args.out.name);
	printf("  Type: 	 %hhu\n", args.out.file_type);
	printf("  Name len:	%hhu\n", args.out.name_len);
	return 0;
}

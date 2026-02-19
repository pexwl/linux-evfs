#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>

int main() {

	FILE *test;
	test = fopen("/tmp/ext4-a0/abc.txt", "w+");
	int fd = fileno(test);
	ioctl(fd, 99);

	return 0;
}

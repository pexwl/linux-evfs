#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef _TEVFS_HELPER
#define _TEVFS_HELPER
#define _TEVFS_EXT4_PATHLEN 255

int usage(const int actual_argc, const int expected_argc, const char usage_str[]) {
	if (actual_argc != expected_argc) {
		fprintf(stderr, "usage: %s\n", usage_str);
		return 1;
	}

	return 0;
}

void null_terminated_strncpy(char * dst, const char * src, size_t n) {
	strncpy(dst, src, n);
	dst[n - 1] = 0;
}

int str2ul(char * str, unsigned int * num, char * var) {
	if (str == NULL) return 1;
	if (num == NULL) return 1;

	char * endptr;
	*num = strtoul(str, &endptr, 10);
	
	if (endptr == str) {
		if (var != NULL) fprintf(stderr, "invalid %s: %s\n", var, str);
		return 1;
	}

	return 0;
}

int str2ull(char * str, unsigned long long * num, char * var) {
	if (str == NULL) return 1;
	if (num == NULL) return 1;

	char * endptr;
	*num = strtoul(str, &endptr, 10);
	
	if (endptr == str) {
		if (var != NULL) fprintf(stderr, "invalid %s: %s\n", var, str);
		return 1;
	}

	return 0;
}

#endif

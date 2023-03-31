#define _DEFAULT_SOURCE

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

#include "syscalls.h"

int main(int argc, char *argv[]) {
	if (argc != 2) {
		printf("usage: %s [orientation]\n", argv[0]);
		return 1;
	}

	int orientation = atoi(argv[1]);
	int ret = syscall(SYS_set_orientation, orientation);
	if (ret) {
		perror("set_orientation");
		return 1;
	}

	puts("OK");
}

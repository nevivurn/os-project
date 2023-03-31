#define _DEFAULT_SOURCE

#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <stdio.h>

#include "syscalls.h"

int main(int argc, char *argv[]) {
	if (argc != 4) {
		puts("invalid args");
		return 0;
	}

	int lo = atoi(argv[1]);
	int hi = atoi(argv[2]);
	int type = atoi(argv[3]);

	long ret = syscall(SYS_rotation_lock, lo, hi, type);
	if (ret == -1) {
		perror("rotation_lock");
		return 1;
	}
	long lid = ret;

	printf("locked(%ld) %d %d %d\n", lid, lo, hi, type);
	getchar();

	ret = syscall(SYS_rotation_unlock, ret);
	if (ret == -1) {
		perror("rotation_unlock");
		return 1;
	}

	printf("unlocked(%ld) %d %d %d\n", lid, lo, hi, type);
}

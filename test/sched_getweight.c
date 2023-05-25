#define _DEFAULT_SOURCE

#include <sys/syscall.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

#include "syscalls.h"

int main(int argc, char *argv[]) {
	if (argc != 2) {
		printf("usage: %s [pid]\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	pid_t pid = atoi(argv[1]);

	long ret = syscall(SYS_sched_getweight, pid);
	if (ret < 0) {
		perror("sched_getweight");
		exit(EXIT_FAILURE);
	}

	printf("%ld\n", ret);
}

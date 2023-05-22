#define _DEFAULT_SOURCE

#include <sys/syscall.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

#include "syscalls.h"

int main(int argc, char *argv[]) {
	if (argc != 3) {
		printf("usage: %s [pid] [weight]\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	pid_t pid = atoi(argv[1]);
	unsigned int weight = atoi(argv[2]);

	long ret = syscall(SYS_sched_setweight, pid, weight);
	if (ret < 0) {
		perror("sched_setweight");
		exit(EXIT_FAILURE);
	}

	printf("%ld\n", ret);
}

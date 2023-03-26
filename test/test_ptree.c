#define _DEFAULT_SOURCE
#include <unistd.h>
#include <sys/syscall.h>
#include <stdlib.h>
#include <stdio.h>

struct pinfo {
	int64_t       state;       /* current state of the process */
	pid_t         pid;         /* process id */
	int64_t       uid;         /* user id of the process owner */
	char          comm[64];    /* name of the program executed */
	unsigned int  depth;       /* depth of the process in the process tree */
};

int main(int argc, char *argv[]) {
	if (argc != 2) {
		fprintf(stderr, "Usage: %s <buffer size>\n", argv[0]);
		return 1;
	}
	int size = atoi(argv[1]);
	struct pinfo *pp = malloc(sizeof(struct pinfo) * size);

	long ret = syscall(294, pp, size);
	if (ret < 0) {
		perror("ptree");
		return 1;
	}

	for (long i = 0; i < ret; i++) {
		struct pinfo p = pp[i];
		for (int d = 0; d < p.depth; d++)
			putchar('\t');
		printf("%s, %d, %lld, %lld\n", p.comm, p.pid, p.state, p.uid);
	}
}

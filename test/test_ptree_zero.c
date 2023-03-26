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

int main(void) {
	struct pinfo pp[100];

	long ret = syscall(294, pp, 0);
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

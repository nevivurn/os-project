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
	long ret = syscall(294, NULL, 100);
	if (ret < 0) {
		perror("ptree");
		return 1;
	}
}

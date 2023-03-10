#ifndef _LINUX_PINFO_H
#define _LINUX_PINFO_H

struct pinfo {
	int64_t       state;       /* current state of the process */
	pid_t         pid;         /* process id */
	int64_t       uid;         /* user id of the process owner */
	char          comm[64];    /* name of the program executed */
	unsigned int  depth;       /* depth of the process in the process tree */
};

#endif

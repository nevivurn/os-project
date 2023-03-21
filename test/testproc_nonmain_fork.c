#define _DEFAULT_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>

void *thread_start(void *arg) {
	pid_t p = fork();
	if (p == -1) {
		perror("fork");
		exit(EXIT_FAILURE);
	}
	if (p != 0) {
		pause();
		return NULL;
	}
	execl("/bin/sh", "sh", "-c", "sleep inf");
}

int main(void) {
	pthread_t tid;
	int ret = pthread_create(&tid, NULL, thread_start, NULL);
	if (ret != 0) {
		perror("pthread_create");
		return EXIT_FAILURE;
	}

	pause();
}

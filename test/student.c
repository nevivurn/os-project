#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>

#include "syscalls.h"

int go = 1;

void term(int signum) {
    go = 0;
}


int main(int argc, char *argv[]) {
    if (argc != 3) {
        puts("invalid args");
        return 1;
    }

    struct sigaction action;
	memset(&action, 0, sizeof(struct sigaction));
	action.sa_handler = term;
	if (sigaction(SIGTERM, &action, NULL) == -1)
		exit(-1);

    int lo = atoi(argv[1]);
    int hi = atoi(argv[2]);

    while(go) {
        long lock_id = syscall(SYS_rotation_lock, lo, hi, ROT_READ);

        if (lock_id == -1) {
            perror("rotation_lock");
            return 1;
        }

        printf("student-%d-%d: ", lo, hi);

        FILE *quiz = fopen("quiz", "r");
        if (quiz) {
            // read content in quiz
            char buf[100];
            fgets(buf, 100, quiz);
            int num = atoi(buf);
            printf("%d = ", num);
            int factor = 2, is_first = 1;
            while(num > 1) {
                if (num % factor == 0) {
                    if (!is_first) {
                        printf(" * ");
                    }
                    printf("%d", factor);
                    is_first = 0;
                    num /= factor;
                } else {
                    factor++;
                }
            }
            printf("\n");
        }

        long ret = syscall(SYS_rotation_unlock, lock_id);
        if (ret == -1) {
            perror("rotation_unlock");
            return 1;
        }
    }
}
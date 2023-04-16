#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>

#include "syscalls.h"

int go = 1;

void term(int signum) {
    go = 0;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        puts("invalid args");
        return 1;
    }

    struct sigaction action;
    memset(&action, 0, sizeof(struct sigaction));
    action.sa_handler = term;
    if (sigaction(SIGTERM, &action, NULL) == -1)
        exit(-1);


    int num = atoi(argv[1]);

    while(go) {
        long lock_id = syscall(SYS_rotation_lock, 0, 180, ROT_WRITE);

        if (lock_id == -1) {
            perror("rotation_lock");
            return 1;
        }

        printf("professor: %d\n", num);

        FILE *quiz = fopen("quiz", "w");
        if (quiz) {
            fprintf(quiz, "%d\n", num);
            fclose(quiz);
        } else {
            perror("fopen");
            return 1;
        }

        long ret = syscall(SYS_rotation_unlock, lock_id);
        if (ret == -1) {
            perror("rotation_unlock");
            return 1;
        }
        num++;
    }
}
